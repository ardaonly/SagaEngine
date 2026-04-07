/// @file WorldState.h
/// @brief Authoritative, serializable entity-component state for one simulation tick.
///
/// WorldState is the single source of truth for what exists in the simulation
/// at a given tick. It owns all entity lifecycle and all component data.
///
/// Design constraints (non-negotiable for a deterministic networked simulation):
///   1. Components must be trivially copyable — raw memcpy serialization.
///   2. Component type IDs must be stable across binaries — use ComponentRegistry.
///   3. Serialization is deterministic: entities sorted by ID, types by TypeId.
///   4. No heap-allocated component fields — pointer stability is not guaranteed
///      after entity creation/destruction.
///
/// Typical server-side per-tick flow:
///   1. Authority::Tick() calls ISimulationSystem::Update(worldState, dt)
///   2. Systems read/write components via worldState.GetComponent<T>()
///   3. Deterministic::Hash(worldState) produces a checksum for divergence detection
///   4. ReplicationManager::Broadcast() calls worldState.Serialize() for delta
///   5. EventLog appends the serialized bytes for persistence / replay
///
/// Thread safety: NOT thread-safe. All access must be from the simulation thread.

#pragma once

#include "SagaEngine/ECS/ComponentRegistry.h"
#include "SagaEngine/ECS/Entity.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <memory>
#include <span>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace SagaEngine::Simulation {

using ECS::ComponentTypeId;
using ECS::EntityId;
using ECS::EntityHandle;
using ECS::EntityVersion;
using ECS::INVALID_ENTITY_ID;

/// Flat, contiguous storage for one component type.
///
/// Data layout: parallel arrays — data[] and entities[] share the same index.
/// Removal uses swap-and-pop for O(1) amortised cost and cache-friendliness.
struct ComponentBlock
{
    ComponentTypeId          typeId{0};     ///< Stable registered ID.
    std::size_t              stride{0};     ///< sizeof(component T).
    std::vector<uint8_t>     data;          ///< Raw component bytes, stride-aligned.
    std::vector<EntityId>    entities;      ///< Parallel owner entity per slot.
    std::unordered_map<EntityId, uint32_t>  entityToSlot; ///< EntityId → slot index.
};

/// Authoritative entity-component world snapshot for one simulation tick.
class WorldState
{
public:
    WorldState() = default;
    ~WorldState() = default;

    WorldState(const WorldState&) = default;
    WorldState& operator=(const WorldState&) = default;
    WorldState(WorldState&&) noexcept = default;
    WorldState& operator=(WorldState&&) noexcept = default;

    // ─── Entity lifecycle ──────────────────────────────────────────────────────

    /// Create a new entity. Returns a versioned handle for stale-reference detection.
    [[nodiscard]] EntityHandle CreateEntity();

    /// Destroy an entity and remove all its components.
    ///
    /// After this call the entity's ID may be reused with an incremented version.
    /// Stale EntityHandles obtained before destruction will fail IsAlive() checks.
    void DestroyEntity(EntityId id);

    /// Return true if the entity exists in this state.
    [[nodiscard]] bool IsAlive(EntityId id) const noexcept;

    /// Return true if the versioned handle is still valid (entity alive, same gen).
    [[nodiscard]] bool IsValid(EntityHandle handle) const noexcept;

    // ─── Component access ──────────────────────────────────────────────────────

    /// Add component T to entity. Replaces any existing component of the same type.
    ///
    /// @returns Reference to the stored component. Valid until the next structural
    ///          change (add/remove component or entity destruction).
    template<typename T>
    T& AddComponent(EntityId id, T component = {});

    /// Return pointer to component T on entity, or nullptr if absent.
    template<typename T>
    [[nodiscard]] T* GetComponent(EntityId id) noexcept;

    template<typename T>
    [[nodiscard]] const T* GetComponent(EntityId id) const noexcept;

    /// Return true if entity has component T.
    template<typename T>
    [[nodiscard]] bool HasComponent(EntityId id) const noexcept;

    /// Remove component T from entity. No-op if entity lacks the component.
    template<typename T>
    void RemoveComponent(EntityId id);

    // ─── Query ─────────────────────────────────────────────────────────────────

    /// Return all alive entities that possess every listed component type.
    ///
    /// The result order is deterministic within a single WorldState instance
    /// (sorted by EntityId) but may differ across serialized/deserialized copies
    /// until the state is fully rebuilt.
    [[nodiscard]] std::vector<EntityId> Query(
        const std::vector<ComponentTypeId>& required) const;

    /// Low-level: return the raw block for a component type, or nullptr.
    [[nodiscard]] const ComponentBlock* GetBlock(ComponentTypeId id) const noexcept;
    [[nodiscard]]       ComponentBlock* GetBlock(ComponentTypeId id)       noexcept;

    /// Return the set of all alive entity IDs (for ECS::Query integration).
    [[nodiscard]] const std::unordered_set<EntityId>& GetAliveEntities() const noexcept
    {
        return m_alive;
    }

    // ─── Serialization ─────────────────────────────────────────────────────────

    /// Serialize the full state to a flat byte buffer.
    ///
    /// Format is deterministic: entities sorted by ID, component types by TypeId.
    /// The output is suitable for hash comparison, persistence, and replication.
    [[nodiscard]] std::vector<uint8_t> Serialize() const;

    /// Deserialize a WorldState from bytes produced by Serialize().
    ///
    /// @throws std::runtime_error on malformed or truncated input.
    [[nodiscard]] static WorldState Deserialize(std::span<const uint8_t> bytes);

    // ─── Hashing ───────────────────────────────────────────────────────────────

    /// Compute a deterministic FNV-1a hash of the serialized state.
    ///
    /// Equal states always produce the same hash. This is the divergence
    /// detection signal used by Deterministic::Verify().
    [[nodiscard]] uint64_t Hash() const noexcept;

    // ─── Diagnostics ───────────────────────────────────────────────────────────

    [[nodiscard]] uint32_t EntityCount()    const noexcept { return static_cast<uint32_t>(m_alive.size()); }
    [[nodiscard]] uint32_t ComponentTypes() const noexcept { return static_cast<uint32_t>(m_blocks.size()); }

    /// Count of total component instances across all types.
    [[nodiscard]] uint32_t TotalComponents() const noexcept;

private:
    // ─── Entity storage ────────────────────────────────────────────────────────

    std::unordered_set<EntityId>   m_alive;    ///< Fast alive-check.
    std::vector<EntityVersion>     m_versions; ///< versions[id] — grows on demand.
    std::vector<EntityId>          m_freeList; ///< Recycled IDs not yet reused.
    EntityId                       m_nextId{1};///< Next ID to issue if free list empty.

    // ─── Component storage ─────────────────────────────────────────────────────

    std::unordered_map<ComponentTypeId, ComponentBlock> m_blocks; ///< One block per type.

    // ─── Helpers ───────────────────────────────────────────────────────────────

    /// Return or create a block for the given type and element stride.
    ComponentBlock& GetOrCreateBlock(ComponentTypeId id, std::size_t stride);

    /// Grow m_versions to accommodate entity id.
    void EnsureVersionCapacity(EntityId id);

    /// Remove entity from every component block.
    void StripAllComponents(EntityId id);

    // ─── Serialization constants ───────────────────────────────────────────────

    static constexpr uint32_t kMagic   = 0x47415357u; ///< "WSAG" little-endian.
    static constexpr uint16_t kVersion = 1u;
};

// ─── Template implementations ─────────────────────────────────────────────────

template<typename T>
T& WorldState::AddComponent(EntityId id, T component)
{
    static_assert(std::is_trivially_copyable_v<T>,
        "WorldState components must be trivially copyable.");

    if (!IsAlive(id))
        throw std::logic_error("WorldState::AddComponent: entity is not alive");

    const ComponentTypeId typeId = ECS::ComponentRegistry::Instance().GetId<T>();
    ComponentBlock& block = GetOrCreateBlock(typeId, sizeof(T));

    const auto it = block.entityToSlot.find(id);
    if (it != block.entityToSlot.end())
    {
        // Overwrite existing component in-place.
        uint8_t* dest = block.data.data() + it->second * sizeof(T);
        std::memcpy(dest, &component, sizeof(T));
        return *reinterpret_cast<T*>(dest);
    }

    // Append new slot.
    const uint32_t slot = static_cast<uint32_t>(block.entities.size());
    block.entities.push_back(id);
    block.data.resize(block.data.size() + sizeof(T));
    uint8_t* dest = block.data.data() + slot * sizeof(T);
    std::memcpy(dest, &component, sizeof(T));
    block.entityToSlot.emplace(id, slot);

    return *reinterpret_cast<T*>(dest);
}

template<typename T>
T* WorldState::GetComponent(EntityId id) noexcept
{
    const ComponentTypeId typeId = ECS::ComponentRegistry::Instance().GetId<T>();
    ComponentBlock* block = GetBlock(typeId);
    if (!block) return nullptr;

    const auto it = block->entityToSlot.find(id);
    if (it == block->entityToSlot.end()) return nullptr;

    return reinterpret_cast<T*>(block->data.data() + it->second * sizeof(T));
}

template<typename T>
const T* WorldState::GetComponent(EntityId id) const noexcept
{
    const ComponentTypeId typeId = ECS::ComponentRegistry::Instance().GetId<T>();
    const ComponentBlock* block = GetBlock(typeId);
    if (!block) return nullptr;

    const auto it = block->entityToSlot.find(id);
    if (it == block->entityToSlot.end()) return nullptr;

    return reinterpret_cast<const T*>(block->data.data() + it->second * sizeof(T));
}

template<typename T>
bool WorldState::HasComponent(EntityId id) const noexcept
{
    const ComponentTypeId typeId = ECS::ComponentRegistry::Instance().GetId<T>();
    const ComponentBlock* block = GetBlock(typeId);
    if (!block) return false;
    return block->entityToSlot.contains(id);
}

template<typename T>
void WorldState::RemoveComponent(EntityId id)
{
    const ComponentTypeId typeId = ECS::ComponentRegistry::Instance().GetId<T>();
    ComponentBlock* block = GetBlock(typeId);
    if (!block) return;

    const auto it = block->entityToSlot.find(id);
    if (it == block->entityToSlot.end()) return;

    const uint32_t slot = it->second;
    const uint32_t last = static_cast<uint32_t>(block->entities.size()) - 1u;

    if (slot != last)
    {
        // Swap-and-pop: move last slot into the removed slot.
        const EntityId movedEntity = block->entities[last];
        block->entities[slot] = movedEntity;

        uint8_t* dst = block->data.data() + slot * sizeof(T);
        const uint8_t* src = block->data.data() + last * sizeof(T);
        std::memcpy(dst, src, sizeof(T));

        block->entityToSlot[movedEntity] = slot;
    }

    block->entities.pop_back();
    block->data.resize(block->data.size() - sizeof(T));
    block->entityToSlot.erase(it);
}

} // namespace SagaEngine::Simulation
