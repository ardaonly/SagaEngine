/// @file GranularWorldHash.cpp
/// @file GranularWorldHash.cpp
/// @brief Subsystem-level and component-level hash for desync debugging.

#include "SagaEngine/Client/Replication/GranularWorldHash.h"

#include "SagaEngine/Client/Replication/EcsSnapshotApply.h"
#include "SagaEngine/Simulation/WorldState.h"

#include <algorithm>
#include <cstdio>

namespace SagaEngine::Client::Replication {

namespace {

/// FNV-1a 64-bit hash constants.
inline constexpr std::uint64_t kFNVOffsetBasis = 14695981039346656037ULL;
inline constexpr std::uint64_t kFNVPrime       = 1099511628211ULL;

[[nodiscard]] std::uint64_t FNV1a_Byte(std::uint64_t hash, std::uint8_t byte) noexcept
{
    hash ^= static_cast<std::uint64_t>(byte);
    hash *= kFNVPrime;
    return hash;
}

[[nodiscard]] std::uint64_t FNV1a_U32(std::uint64_t hash, std::uint32_t val) noexcept
{
    hash = FNV1a_Byte(hash, static_cast<std::uint8_t>(val));
    hash = FNV1a_Byte(hash, static_cast<std::uint8_t>(val >> 8));
    hash = FNV1a_Byte(hash, static_cast<std::uint8_t>(val >> 16));
    hash = FNV1a_Byte(hash, static_cast<std::uint8_t>(val >> 24));
    return hash;
}

/// Collect all alive entity IDs sorted ascending.
[[nodiscard]] std::vector<ECS::EntityId> SortedEntities(const Simulation::WorldState& world) noexcept
{
    const auto& alive = world.GetAliveEntities();
    std::vector<ECS::EntityId> result(alive.begin(), alive.end());
    std::sort(result.begin(), result.end());
    return result;
}

} // namespace

// ─── Granular hashes ────────────────────────────────────────────────────────

GranularHashResult ComputeGranularHashes(
    const Simulation::WorldState& world,
    const AuthorityTable& authority) noexcept
{
    GranularHashResult result;

    auto entities = SortedEntities(world);
    std::uint64_t worldHash = kFNVOffsetBasis;

    // Collect all component type IDs that are not ClientOnly.
    std::vector<ECS::ComponentTypeId> activeTypes;
    // Iterate over a reasonable range of type IDs.
    for (std::uint32_t tid = 0; tid < AuthorityTable::kMaxComponentTypes; ++tid)
    {
        if (authority.Get(tid) == ReplicationAuthority::ClientOnly)
            continue;
        const auto* block = world.GetBlock(tid);
        if (!block || block->entities.empty())
            continue;
        activeTypes.push_back(tid);
    }

    // Compute per-subsystem hashes.
    result.subsystemHashes.reserve(activeTypes.size());
    for (ECS::ComponentTypeId typeId : activeTypes)
    {
        const auto* block = world.GetBlock(typeId);
        if (!block)
            continue;

        std::uint64_t subsystemHash = kFNVOffsetBasis;
        std::uint32_t entityCount = 0;

        // Hash each entity's component bytes, sorted by entityId.
        // We need to iterate entities in sorted order for determinism.
        std::vector<std::pair<ECS::EntityId, std::uint32_t>> entitySlots;
        entitySlots.reserve(block->entities.size());
        for (std::uint32_t slot = 0; slot < block->entities.size(); ++slot)
            entitySlots.push_back({block->entities[slot], slot});
        std::sort(entitySlots.begin(), entitySlots.end());

        for (const auto& [entityId, slot] : entitySlots)
        {
            subsystemHash = FNV1a_U32(subsystemHash, static_cast<std::uint32_t>(entityId));
            subsystemHash = FNV1a_U32(subsystemHash, typeId);
            subsystemHash = FNV1a_U32(subsystemHash, static_cast<std::uint32_t>(block->stride));

            const std::uint8_t* data = block->data.data() + slot * block->stride;
            for (std::size_t b = 0; b < block->stride; ++b)
                subsystemHash = FNV1a_Byte(subsystemHash, data[b]);

            // Also fold into the world hash.
            worldHash = FNV1a_U32(worldHash, static_cast<std::uint32_t>(entityId));
            worldHash = FNV1a_U32(worldHash, typeId);
            worldHash = FNV1a_U32(worldHash, static_cast<std::uint32_t>(block->stride));
            for (std::size_t b = 0; b < block->stride; ++b)
                worldHash = FNV1a_Byte(worldHash, data[b]);

            ++entityCount;
        }

        SubsystemHashEntry entry;
        entry.typeId = typeId;
        entry.hash = subsystemHash;
        entry.entityCount = entityCount;
        result.subsystemHashes.push_back(entry);
    }

    result.worldHash = worldHash;
    result.matches = true;

    return result;
}

// ─── Entity component diff ──────────────────────────────────────────────────

std::vector<ComponentByteDiff> DiffEntityComponents(
    const Simulation::WorldState& clientWorld,
    const std::uint8_t* serverData,
    std::size_t serverSize,
    ECS::EntityId entityId) noexcept
{
    std::vector<ComponentByteDiff> diffs;

    // Iterate all component types the entity has on the client.
    for (std::uint32_t tid = 0; tid < AuthorityTable::kMaxComponentTypes; ++tid)
    {
        const auto* block = clientWorld.GetBlock(tid);
        if (!block)
            continue;

        auto it = block->entityToSlot.find(entityId);
        if (it == block->entityToSlot.end())
            continue;

        std::uint32_t slot = it->second;
        const std::uint8_t* clientBytes = block->data.data() + slot * block->stride;

        // Find corresponding server bytes.
        // The server data format is: [entityId(4)][componentCount(2)]
        // then per component: [typeId(2)][dataLen(2)][data:N]
        // For simplicity, scan the server blob for this typeId.
        // A production system would have a direct index.
        const std::uint8_t* serverBytes = nullptr;
        std::size_t serverDataLen = 0;

        std::size_t offset = 0;
        while (offset + 6 <= serverSize)
        {
            std::uint32_t srvEntityId = 0;
            std::uint16_t srvCompCount = 0;
            std::memcpy(&srvEntityId, serverData + offset, 4);
            std::memcpy(&srvCompCount, serverData + offset + 4, 2);
            offset += 6;

            if (srvEntityId != entityId)
            {
                // Skip this entity's components.
                for (std::uint16_t c = 0; c < srvCompCount && offset + 4 <= serverSize; ++c)
                {
                    std::uint16_t dataLen = 0;
                    std::memcpy(&dataLen, serverData + offset + 2, 2);
                    offset += 4 + dataLen;
                }
                continue;
            }

            // Found the entity — scan its components.
            for (std::uint16_t c = 0; c < srvCompCount && offset + 4 <= serverSize; ++c)
            {
                std::uint16_t srvTypeId = 0, srvDataLen = 0;
                std::memcpy(&srvTypeId, serverData + offset, 2);
                std::memcpy(&srvDataLen, serverData + offset + 2, 2);
                offset += 4;

                if (srvTypeId == tid && srvDataLen == block->stride)
                {
                    serverBytes = serverData + offset;
                    serverDataLen = srvDataLen;
                }
                offset += srvDataLen;
            }
            break;
        }

        if (!serverBytes || serverDataLen != block->stride)
            continue;

        // Compare byte by byte.
        for (std::size_t b = 0; b < block->stride; ++b)
        {
            if (clientBytes[b] != serverBytes[b])
            {
                ComponentByteDiff diff;
                diff.typeId = tid;
                diff.entityId = entityId;
                diff.byteOffset = static_cast<std::uint32_t>(b);
                diff.clientByte = clientBytes[b];
                diff.serverByte = serverBytes[b];
                diffs.push_back(diff);
            }
        }
    }

    return diffs;
}

// ─── Format report ──────────────────────────────────────────────────────────

std::string FormatHashReport(const GranularHashResult& result,
                              std::uint64_t serverWorldHash) noexcept
{
    char buf[4096];
    int pos = 0;

    pos += std::snprintf(buf + pos, sizeof(buf) - pos,
        "World hash: 0x%016llX", static_cast<unsigned long long>(result.worldHash));

    if (result.worldHash != serverWorldHash)
    {
        pos += std::snprintf(buf + pos, sizeof(buf) - pos,
            " (MISMATCH — server=0x%016llX)", static_cast<unsigned long long>(serverWorldHash));
    }
    else
    {
        pos += std::snprintf(buf + pos, sizeof(buf) - pos, " (ok)");
    }
    pos += std::snprintf(buf + pos, sizeof(buf) - pos, "\n");

    pos += std::snprintf(buf + pos, sizeof(buf) - pos, "Subsystem breakdown:\n");

    for (const auto& entry : result.subsystemHashes)
    {
        pos += std::snprintf(buf + pos, sizeof(buf) - pos,
            "  typeId=%u entities=%u hash=0x%016llX\n",
            static_cast<unsigned>(entry.typeId),
            static_cast<unsigned>(entry.entityCount),
            static_cast<unsigned long long>(entry.hash));
    }

    return std::string(buf, static_cast<std::size_t>(pos));
}

} // namespace SagaEngine::Client::Replication
