/// @file EcsSnapshotApply.cpp
/// @brief Authority-aware ECS apply functions for replication pipeline.

#include "SagaEngine/Client/Replication/EcsSnapshotApply.h"

#include "SagaEngine/ECS/ComponentSerializerRegistry.h"
#include "SagaEngine/Simulation/WorldState.h"

#include "SagaEngine/Core/Log/Log.h"

#include <algorithm>
#include <cstring>

namespace SagaEngine::Client::Replication {

namespace {

constexpr const char* kLogTag = "Replication";

} // namespace

// ─── Authority table ───────────────────────────────────────────────────────

AuthorityTable g_AuthorityTable;

const char* AuthorityToString(ReplicationAuthority auth) noexcept
{
    switch (auth)
    {
        case ReplicationAuthority::ServerOwned:     return "ServerOwned";
        case ReplicationAuthority::ClientPredicted: return "ClientPredicted";
        case ReplicationAuthority::ClientOnly:      return "ClientOnly";
        default:                                    return "Unknown";
    }
}

void AuthorityTable::Register(ECS::ComponentTypeId typeId, ReplicationAuthority auth) noexcept
{
    if (typeId < kMaxComponentTypes)
        table_[typeId] = static_cast<std::uint8_t>(auth);
}

ReplicationAuthority AuthorityTable::Get(ECS::ComponentTypeId typeId) const noexcept
{
    if (typeId >= kMaxComponentTypes)
        return ReplicationAuthority::ClientOnly;
    return static_cast<ReplicationAuthority>(table_[typeId]);
}

void AuthorityTable::Reset() noexcept
{
    std::memset(table_, static_cast<std::uint8_t>(ReplicationAuthority::ClientOnly), sizeof(table_));
}

// ─── Full snapshot apply ───────────────────────────────────────────────────

FullSnapshotApplyFn MakeFullSnapshotApplyFn(const AuthorityTable& authority)
{
    return [&authority](Simulation::WorldState& world, const DecodedWorldSnapshot& snapshot) -> bool {
        // Decode the snapshot payload using the wire format:
        //   [entityId:4][componentCount:2]
        //   [Per Component: typeId(2) | dataLen(2) | data(N)]
        const std::uint8_t* payload = snapshot.payload.data();
        std::size_t remaining = snapshot.payload.size();

        // Track which entity IDs the server sent for despawning stale client entities.
        std::vector<std::uint32_t> serverEntityIds;
        serverEntityIds.reserve(snapshot.entityCount);

        while (remaining >= 6)
        {
            std::uint32_t entityId = 0;
            std::uint16_t compCount = 0;
            std::memcpy(&entityId, payload, 4);
            std::memcpy(&compCount, payload + 4, 2);
            payload += 6;
            remaining -= 6;

            serverEntityIds.push_back(entityId);

            // Create the entity in the WorldState.
            // Check if it already exists first.
            bool entityExists = world.IsAlive(entityId);
            if (!entityExists)
            {
                auto handle = world.CreateEntity();
                if (handle.id != entityId)
                {
                    // The ECS assigned a different ID — this shouldn't happen
                    // if server and client both start from ID 1 sequentially.
                    LOG_WARN(kLogTag,
                             "EcsSnapshotApply: entity ID mismatch: expected %u, got %u",
                             entityId, handle.id);
                }
            }

            for (std::uint16_t c = 0; c < compCount; ++c)
            {
                if (remaining < 4)
                {
                    LOG_ERROR(kLogTag,
                              "EcsSnapshotApply: component header truncated "
                              "(remaining=%zu, need=4)",
                              remaining);
                    return false;
                }

                std::uint16_t typeId = 0, dataLen = 0;
                std::memcpy(&typeId, payload, 2);
                std::memcpy(&dataLen, payload + 2, 2);
                payload += 4;
                remaining -= 4;

                if (dataLen > remaining)
                {
                    LOG_ERROR(kLogTag,
                              "EcsSnapshotApply: component dataLen %u exceeds remaining %zu",
                              static_cast<unsigned>(dataLen), remaining);
                    return false;
                }

                // Check authority — skip ClientOnly components.
                ReplicationAuthority auth = authority.Get(typeId);
                if (auth == ReplicationAuthority::ClientOnly)
                {
                    payload += dataLen;
                    remaining -= dataLen;
                    continue;
                }

                // Write raw component bytes into the WorldState block.
                Simulation::ComponentBlock* block = world.GetBlock(typeId);
                if (!block)
                {
                    // Block doesn't exist — create it now.
                    // This happens on the first snapshot for each component type.
                    block = &world.GetOrCreateBlockRaw(typeId, static_cast<std::size_t>(dataLen));
                }

                if (block->stride != dataLen)
                {
                    LOG_WARN(kLogTag,
                             "EcsSnapshotApply: component type %u stride mismatch: "
                             "expected %zu, got %u",
                             static_cast<unsigned>(typeId), block->stride,
                             static_cast<unsigned>(dataLen));
                    payload += dataLen;
                    remaining -= dataLen;
                    continue;
                }

                // Find or create slot for this entity.
                auto it = block->entityToSlot.find(entityId);
                if (it != block->entityToSlot.end())
                {
                    // Overwrite existing component in-place.
                    std::uint8_t* dest = block->data.data() + it->second * block->stride;
                    std::memcpy(dest, payload, dataLen);
                }
                else
                {
                    // Append new component slot.
                    std::uint32_t slot = static_cast<std::uint32_t>(block->entities.size());
                    block->entities.push_back(entityId);
                    block->data.resize(block->data.size() + block->stride);
                    std::uint8_t* dest = block->data.data() + slot * block->stride;
                    std::memcpy(dest, payload, dataLen);
                    block->entityToSlot.emplace(entityId, slot);
                }

                payload += dataLen;
                remaining -= dataLen;
            }
        }

        // Despawn client entities that the server no longer sends.
        // This is a simple approach: for a full snapshot, any alive entity
        // on the client that isn't in the server's list should be removed.
        // NOTE: This is O(N*M) — fine for test, needs optimization for production.
        auto aliveEntities = world.GetAliveEntities();
        for (auto eid : aliveEntities)
        {
            bool found = false;
            for (auto sid : serverEntityIds)
            {
                if (eid == sid) { found = true; break; }
            }
            if (!found)
            {
                world.DestroyEntity(eid);
            }
        }

        return true;
    };
}

// ─── Delta snapshot apply ──────────────────────────────────────────────────

DeltaSnapshotApplyFn MakeDeltaSnapshotApplyFn(const AuthorityTable& authority)
{
    return [&authority](Simulation::WorldState& world, const DecodedDeltaSnapshot& delta) -> bool {
        // Decode the delta payload into a PatchJournal.
        PatchJournal journal;

        // The delta payload uses the wire format:
        // [entityId:4][componentCount:2][typeId:2][dataLen:2][data:N]...
        const std::uint8_t* payload = delta.payload.data();
        std::size_t remaining = delta.payload.size();

        while (remaining >= 6)  // Minimum: entityId(4) + componentCount(2).
        {
            std::uint32_t entityId = 0;
            std::uint16_t compCount = 0;
            std::memcpy(&entityId, payload, 4);
            std::memcpy(&compCount, payload + 4, 2);
            payload += 6;
            remaining -= 6;

            // Tombstone check.
            if (compCount == 0xFFFF)
            {
                ComponentRemovePatch removePatch{};
                removePatch.entityId = entityId;
                journal.PushDespawn({entityId, 0});
                continue;
            }

            for (std::uint16_t c = 0; c < compCount && remaining >= 4; ++c)
            {
                std::uint16_t typeId = 0, dataLen = 0;
                std::memcpy(&typeId, payload, 2);
                std::memcpy(&dataLen, payload + 2, 2);
                payload += 4;
                remaining -= 4;

                if (dataLen > remaining)
                {
                    LOG_ERROR(kLogTag,
                              "EcsSnapshotApply: delta component dataLen %u exceeds remaining %zu",
                              static_cast<unsigned>(dataLen), remaining);
                    return false;
                }

                // Check authority.
                ReplicationAuthority auth = authority.Get(typeId);
                if (auth == ReplicationAuthority::ClientOnly)
                {
                    // Skip — server should never send ClientOnly updates.
                    payload += dataLen;
                    remaining -= dataLen;
                    continue;
                }

                ComponentWritePatch writePatch{};
                writePatch.entityId = entityId;
                writePatch.typeId   = typeId;
                writePatch.data     = payload;
                writePatch.dataLen  = dataLen;
                journal.PushWrite(writePatch);

                payload += dataLen;
                remaining -= dataLen;
            }
        }

        // Two-phase apply: sort → validate → commit.
        ValidateResult vr = journal.Apply(world);
        if (vr != ValidateResult::Ok)
        {
            LOG_WARN(kLogTag,
                     "EcsSnapshotApply: patch journal apply failed (result=%d)",
                     static_cast<int>(vr));
            return false;
        }

        return true;
    };
}

// ─── Canonical world hash ──────────────────────────────────────────────────

std::uint64_t ComputeCanonicalWorldHash(
    const Simulation::WorldState& world,
    const AuthorityTable& authority) noexcept
{
    // The simplest correct approach: serialize the world state and
    // hash the resulting bytes.  WorldState::Serialize() is
    // deterministic (entities sorted by ID, types by TypeId) so
    // identical states always produce identical hashes.
    //
    // For fine-grained subsystem-level hashing, use GranularWorldHash
    // instead.
    (void)authority;  // Serialize() already excludes ClientOnly components
                      // if they are registered as such.

    auto bytes = world.Serialize();

    // FNV-1a 64-bit hash.
    std::uint64_t hash = 14695981039346656037ULL;
    for (std::uint8_t b : bytes)
    {
        hash ^= static_cast<std::uint64_t>(b);
        hash *= 1099511628211ULL;
    }
    return hash;
}

} // namespace SagaEngine::Client::Replication
