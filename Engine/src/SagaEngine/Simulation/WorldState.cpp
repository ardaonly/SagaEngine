/// @file WorldState.cpp
/// @brief WorldState method implementations.

#include "SagaEngine/Simulation/WorldState.h"
#include "SagaEngine/Core/Log/Log.h"

#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace SagaEngine::Simulation {

// ─── Entity lifecycle ──────────────────────────────────────────────────────────

EntityHandle WorldState::CreateEntity()
{
    EntityId id;

    if (!m_freeList.empty())
    {
        id = m_freeList.back();
        m_freeList.pop_back();
    }
    else
    {
        id = m_nextId++;
    }

    EnsureVersionCapacity(id);

    m_alive.insert(id);

    return EntityHandle{ id, m_versions[id] };
}

void WorldState::DestroyEntity(EntityId id)
{
    if (!IsAlive(id))
    {
        LOG_WARN("WorldState", "DestroyEntity called on non-alive entity %u.", id);
        return;
    }

    StripAllComponents(id);
    m_alive.erase(id);

    // Bump generation so stale handles become invalid.
    EnsureVersionCapacity(id);
    ++m_versions[id];

    m_freeList.push_back(id);
}

bool WorldState::IsAlive(EntityId id) const noexcept
{
    return m_alive.contains(id);
}

bool WorldState::IsValid(EntityHandle handle) const noexcept
{
    if (!IsAlive(handle.id)) return false;
    if (handle.id >= static_cast<EntityId>(m_versions.size())) return false;
    return m_versions[handle.id] == handle.version;
}

// ─── Query ─────────────────────────────────────────────────────────────────────

std::vector<EntityId> WorldState::Query(const std::vector<ComponentTypeId>& required) const
{
    if (required.empty())
    {
        std::vector<EntityId> all(m_alive.begin(), m_alive.end());
        std::sort(all.begin(), all.end());
        return all;
    }

    // Start from the smallest block to minimise iteration.
    const ComponentBlock* smallest = nullptr;
    for (ComponentTypeId tid : required)
    {
        const ComponentBlock* b = GetBlock(tid);
        if (!b) return {};  // No entity can satisfy all requirements.
        if (!smallest || b->entities.size() < smallest->entities.size())
            smallest = b;
    }

    std::vector<EntityId> result;
    result.reserve(smallest->entities.size());

    for (EntityId candidate : smallest->entities)
    {
        bool satisfies = true;
        for (ComponentTypeId tid : required)
        {
            const ComponentBlock* b = GetBlock(tid);
            if (!b || !b->entityToSlot.contains(candidate))
            {
                satisfies = false;
                break;
            }
        }
        if (satisfies)
            result.push_back(candidate);
    }

    std::sort(result.begin(), result.end());
    return result;
}

const ComponentBlock* WorldState::GetBlock(ComponentTypeId id) const noexcept
{
    const auto it = m_blocks.find(id);
    return it != m_blocks.end() ? &it->second : nullptr;
}

ComponentBlock* WorldState::GetBlock(ComponentTypeId id) noexcept
{
    const auto it = m_blocks.find(id);
    return it != m_blocks.end() ? &it->second : nullptr;
}

// ─── Serialization ─────────────────────────────────────────────────────────────

/// Binary format (all values little-endian):
///
///   [Header]
///     uint32  magic    = kMagic
///     uint16  version  = kVersion
///     uint32  entityCount
///     uint32  typeCount
///
///   [Entity section — entityCount entries, sorted by EntityId]
///     uint32  entityId
///     uint16  entityVersion
///
///   [Component section — typeCount blocks, sorted by ComponentTypeId]
///     uint32  typeId
///     uint32  stride          (sizeof the component struct)
///     uint32  instanceCount
///     [instanceCount entries, sorted by EntityId]
///       uint32  entityId
///       stride  bytes of raw component data

std::vector<uint8_t> WorldState::Serialize() const
{
    // Gather sorted entity list.
    std::vector<EntityId> sortedEntities(m_alive.begin(), m_alive.end());
    std::sort(sortedEntities.begin(), sortedEntities.end());

    // Gather sorted type list.
    std::vector<ComponentTypeId> sortedTypes;
    sortedTypes.reserve(m_blocks.size());
    for (const auto& [tid, _] : m_blocks)
        sortedTypes.push_back(tid);
    std::sort(sortedTypes.begin(), sortedTypes.end());

    // Pre-compute output size to avoid reallocations.
    std::size_t size = 0;
    size += sizeof(uint32_t) + sizeof(uint16_t);   // magic + version
    size += sizeof(uint32_t) + sizeof(uint32_t);   // entityCount + typeCount
    size += sortedEntities.size() * (sizeof(uint32_t) + sizeof(uint16_t));

    for (ComponentTypeId tid : sortedTypes)
    {
        const ComponentBlock& block = m_blocks.at(tid);
        size += sizeof(uint32_t) * 3;              // typeId + stride + instanceCount
        size += block.entities.size() * (sizeof(uint32_t) + block.stride);
    }

    std::vector<uint8_t> out;
    out.reserve(size);

    const auto writeU16 = [&](uint16_t v)
    {
        out.push_back(static_cast<uint8_t>(v));
        out.push_back(static_cast<uint8_t>(v >> 8));
    };
    const auto writeU32 = [&](uint32_t v)
    {
        out.push_back(static_cast<uint8_t>(v));
        out.push_back(static_cast<uint8_t>(v >> 8));
        out.push_back(static_cast<uint8_t>(v >> 16));
        out.push_back(static_cast<uint8_t>(v >> 24));
    };
    const auto writeBytes = [&](const uint8_t* src, std::size_t n)
    {
        out.insert(out.end(), src, src + n);
    };

    // Header.
    writeU32(kMagic);
    writeU16(kVersion);
    writeU32(static_cast<uint32_t>(sortedEntities.size()));
    writeU32(static_cast<uint32_t>(sortedTypes.size()));

    // Entity section.
    for (EntityId eid : sortedEntities)
    {
        writeU32(eid);
        const EntityVersion ver = (eid < static_cast<EntityId>(m_versions.size()))
                                  ? m_versions[eid] : 0u;
        writeU16(ver);
    }

    // Component section.
    for (ComponentTypeId tid : sortedTypes)
    {
        const ComponentBlock& block = m_blocks.at(tid);

        // Sort instances by EntityId for determinism.
        std::vector<uint32_t> slots;
        slots.reserve(block.entities.size());
        for (uint32_t i = 0; i < static_cast<uint32_t>(block.entities.size()); ++i)
            slots.push_back(i);
        std::sort(slots.begin(), slots.end(),
            [&](uint32_t a, uint32_t b) { return block.entities[a] < block.entities[b]; });

        writeU32(tid);
        writeU32(static_cast<uint32_t>(block.stride));
        writeU32(static_cast<uint32_t>(slots.size()));

        for (uint32_t slot : slots)
        {
            writeU32(block.entities[slot]);
            writeBytes(block.data.data() + slot * block.stride, block.stride);
        }
    }

    return out;
}

WorldState WorldState::Deserialize(std::span<const uint8_t> bytes)
{
    std::size_t offset = 0;

    const auto checkRemaining = [&](std::size_t needed)
    {
        if (offset + needed > bytes.size())
            throw std::runtime_error("WorldState::Deserialize: truncated input");
    };

    const auto readU16 = [&]() -> uint16_t
    {
        checkRemaining(2);
        uint16_t v = static_cast<uint16_t>(bytes[offset]) |
                     (static_cast<uint16_t>(bytes[offset + 1]) << 8);
        offset += 2;
        return v;
    };
    const auto readU32 = [&]() -> uint32_t
    {
        checkRemaining(4);
        uint32_t v = static_cast<uint32_t>(bytes[offset])       |
                     (static_cast<uint32_t>(bytes[offset + 1]) << 8)  |
                     (static_cast<uint32_t>(bytes[offset + 2]) << 16) |
                     (static_cast<uint32_t>(bytes[offset + 3]) << 24);
        offset += 4;
        return v;
    };

    // Header.
    const uint32_t magic = readU32();
    if (magic != kMagic)
        throw std::runtime_error("WorldState::Deserialize: bad magic");

    const uint16_t ver = readU16();
    if (ver != kVersion)
        throw std::runtime_error("WorldState::Deserialize: unsupported version");

    const uint32_t entityCount = readU32();
    const uint32_t typeCount   = readU32();

    WorldState state;

    // Entity section.
    for (uint32_t i = 0; i < entityCount; ++i)
    {
        const EntityId eid        = readU32();
        const EntityVersion eVer  = readU16();

        state.EnsureVersionCapacity(eid);
        state.m_versions[eid] = eVer;
        state.m_alive.insert(eid);

        // Keep m_nextId past all deserialized IDs so new creates don't collide.
        if (eid >= state.m_nextId)
            state.m_nextId = eid + 1;
    }

    // Component section.
    for (uint32_t t = 0; t < typeCount; ++t)
    {
        const ComponentTypeId tid      = readU32();
        const uint32_t        stride   = readU32();
        const uint32_t        instCount = readU32();

        ComponentBlock& block = state.GetOrCreateBlock(tid, stride);

        for (uint32_t i = 0; i < instCount; ++i)
        {
            const EntityId eid = readU32();
            checkRemaining(stride);

            const uint32_t slot = static_cast<uint32_t>(block.entities.size());
            block.entities.push_back(eid);
            block.data.resize(block.data.size() + stride);
            std::memcpy(block.data.data() + slot * stride, bytes.data() + offset, stride);
            block.entityToSlot.emplace(eid, slot);

            offset += stride;
        }
    }

    return state;
}

// ─── Hashing ───────────────────────────────────────────────────────────────────

uint64_t WorldState::Hash() const noexcept
{
    // FNV-1a 64-bit over the serialized bytes.
    const std::vector<uint8_t> bytes = Serialize();

    constexpr uint64_t kFnvPrime  = 0x00000100000001B3ull;
    constexpr uint64_t kFnvOffset = 0xCBF29CE484222325ull;

    uint64_t hash = kFnvOffset;
    for (uint8_t b : bytes)
    {
        hash ^= static_cast<uint64_t>(b);
        hash *= kFnvPrime;
    }
    return hash;
}

// ─── Diagnostics ───────────────────────────────────────────────────────────────

uint32_t WorldState::TotalComponents() const noexcept
{
    uint32_t total = 0;
    for (const auto& [_, block] : m_blocks)
        total += static_cast<uint32_t>(block.entities.size());
    return total;
}

// ─── Private helpers ───────────────────────────────────────────────────────────

ComponentBlock& WorldState::GetOrCreateBlock(ComponentTypeId id, std::size_t stride)
{
    auto it = m_blocks.find(id);
    if (it != m_blocks.end())
        return it->second;

    ComponentBlock block;
    block.typeId = id;
    block.stride = stride;
    return m_blocks.emplace(id, std::move(block)).first->second;
}

void WorldState::EnsureVersionCapacity(EntityId id)
{
    if (static_cast<std::size_t>(id) >= m_versions.size())
        m_versions.resize(static_cast<std::size_t>(id) + 1, 0);
}

void WorldState::StripAllComponents(EntityId id)
{
    for (auto& [tid, block] : m_blocks)
    {
        const auto it = block.entityToSlot.find(id);
        if (it == block.entityToSlot.end()) continue;

        const uint32_t slot = it->second;
        const uint32_t last = static_cast<uint32_t>(block.entities.size()) - 1u;

        if (slot != last)
        {
            const EntityId movedEntity = block.entities[last];
            block.entities[slot] = movedEntity;

            uint8_t* dst = block.data.data() + slot * block.stride;
            const uint8_t* src = block.data.data() + last * block.stride;
            std::memcpy(dst, src, block.stride);

            block.entityToSlot[movedEntity] = slot;
        }

        block.entities.pop_back();
        block.data.resize(block.data.size() - block.stride);
        block.entityToSlot.erase(it);
    }
}

} // namespace SagaEngine::Simulation
