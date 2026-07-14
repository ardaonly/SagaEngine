/// @file ReplicationState.h
/// @brief Per-entity component dirty tracking, baseline versioning, and ACK management.
///
/// Layer  : Server / Networking / Replication
/// Purpose: ReplicationState maintains the per-entity, per-client replication
///          bookkeeping needed for delta snapshot computation. Each EntityState
///          tracks a dirty bitmask (one bit per component type), a baseline
///          sequence number (the last tick the client ACKed), and per-component
///          version counters for change detection without full serialization.
///
/// Design:
///   - ComponentTypeId values are dense integers (0–63) — fits in a uint64_t mask.
///   - Each client maintains a separate EntityState per replicated entity.
///   - Sequence numbers flow: server increments per GatherDirty tick, client ACKs
///     via ReplicationAck packet, server advances baseline and can free history.

#pragma once

#include "SagaEngine/ECS/Entity.h"
#include "SagaEngine/ECS/Component.h"
#include "SagaServer/Networking/Core/NetworkTypes.h"

#include <array>
#include <atomic>
#include <cstdint>
#include <limits>
#include <mutex>
#include <optional>
#include <unordered_map>
#include <vector>

namespace SagaEngine::Networking::Replication
{

using EntityId       = ECS::EntityId;
using ComponentTypeId = ECS::ComponentTypeId;
using ClientId       = ::SagaEngine::Networking::ClientId;

// ─── Constants ────────────────────────────────────────────────────────────────

static constexpr uint32_t kMaxComponentTypes = 64; ///< Max distinct component types
static constexpr uint32_t kSequenceHistoryDepth = 64; ///< Unacked history window

// ─── ComponentDirtyMask ───────────────────────────────────────────────────────

/// 64-bit bitmask over component type IDs.  Bit N set ↔ component N is dirty.
struct ComponentDirtyMask
{
    uint64_t bits{0};

    void Mark(ComponentTypeId typeId) noexcept
    {
        if (typeId < kMaxComponentTypes)
            bits |= (UINT64_C(1) << typeId);
    }

    void Clear(ComponentTypeId typeId) noexcept
    {
        if (typeId < kMaxComponentTypes)
            bits &= ~(UINT64_C(1) << typeId);
    }

    void ClearAll() noexcept { bits = 0; }

    [[nodiscard]] bool IsSet(ComponentTypeId typeId) const noexcept
    {
        return typeId < kMaxComponentTypes && (bits & (UINT64_C(1) << typeId)) != 0;
    }

    [[nodiscard]] bool AnyDirty() const noexcept { return bits != 0; }

    [[nodiscard]] std::vector<ComponentTypeId> DirtyComponents() const
    {
        std::vector<ComponentTypeId> result;
        result.reserve(8);
        for (uint32_t i = 0; i < kMaxComponentTypes; ++i)
            if (bits & (UINT64_C(1) << i))
                result.push_back(static_cast<ComponentTypeId>(i));
        return result;
    }

    /// Merge dirty bits from another mask.
    void Merge(const ComponentDirtyMask& other) noexcept { bits |= other.bits; }

    ComponentDirtyMask operator|(const ComponentDirtyMask& o) const noexcept
    {
        return ComponentDirtyMask{bits | o.bits};
    }
};

// ─── ComponentVersion ─────────────────────────────────────────────────────────

/// Per-component version counter to detect changes without full comparison.
using ComponentVersion = uint32_t;

static constexpr ComponentVersion kInvalidVersion = std::numeric_limits<uint32_t>::max();

// ─── EntityState ──────────────────────────────────────────────────────────────

/// Per-entity replication state from the perspective of one client.
struct EntityState
{
    EntityId          entityId{0};

    /// Components the client has acknowledged receiving at baseline.
    ComponentDirtyMask baselineMask;

    /// Components dirty since the last sent snapshot (not yet ACKed).
    ComponentDirtyMask dirtyMask;

    /// Per-component version at the time it was last sent to this client.
    std::array<ComponentVersion, kMaxComponentTypes> sentVersions;

    /// Sequence number of the snapshot in which each component was last sent.
    std::array<uint64_t, kMaxComponentTypes> sentInSequence;

    bool isNew{true}; ///< True until the client has received the first full snapshot

    EntityState()
    {
        sentVersions.fill(kInvalidVersion);
        sentInSequence.fill(0);
    }

    /// Mark a component dirty and update the sent-version expectation.
    void MarkDirty(ComponentTypeId typeId, ComponentVersion newVersion) noexcept
    {
        dirtyMask.Mark(typeId);
        if (typeId < kMaxComponentTypes)
            sentVersions[typeId] = newVersion;
    }

    /// Called after a snapshot containing typeId has been sent at sequence seq.
    void OnSent(ComponentTypeId typeId, uint64_t seq) noexcept
    {
        dirtyMask.Clear(typeId);
        if (typeId < kMaxComponentTypes)
            sentInSequence[typeId] = seq;
    }

    /// Called when the client ACKs sequence seq — advance baseline.
    void OnAck(uint64_t seq) noexcept
    {
        for (uint32_t i = 0; i < kMaxComponentTypes; ++i)
        {
            if (sentInSequence[i] <= seq)
                baselineMask.Mark(static_cast<ComponentTypeId>(i));
        }
        isNew = false;
    }

    /// True if the entity needs any replication work for this client.
    [[nodiscard]] bool NeedsUpdate() const noexcept
    {
        return isNew || dirtyMask.AnyDirty();
    }
};

// ─── ClientReplicationRecord ──────────────────────────────────────────────────

/// All replication bookkeeping for one client across all entities.
struct ClientReplicationRecord
{
    ClientId clientId{0};

    std::unordered_map<EntityId, EntityState> entityStates;

    /// Monotonic snapshot sequence counter — incremented on each GatherDirty.
    uint64_t currentSequence{0};

    /// Last sequence number acknowledged by the client.
    uint64_t lastAckedSequence{0};

    /// Cumulative bandwidth budget consumed this tick (bytes).
    uint32_t bandwidthUsedThisTick{0};

    /// Per-tick bandwidth budget cap (bytes). 0 = uncapped.
    uint32_t bandwidthBudgetPerTick{0};

    /// Ensure EntityState exists for entityId, returning a reference.
    EntityState& GetOrCreate(EntityId entityId)
    {
        auto [it, inserted] = entityStates.try_emplace(entityId);
        if (inserted)
            it->second.entityId = entityId;
        return it->second;
    }

    /// Remove entity tracking (entity destroyed or out of interest).
    void RemoveEntity(EntityId entityId)
    {
        entityStates.erase(entityId);
    }

    void ResetBandwidthAccumulator() noexcept
    {
        bandwidthUsedThisTick = 0;
    }

    [[nodiscard]] bool HasBandwidthBudget(uint32_t bytes) const noexcept
    {
        if (bandwidthBudgetPerTick == 0)
            return true;
        return (bandwidthUsedThisTick + bytes) <= bandwidthBudgetPerTick;
    }

    void ConsumeBandwidth(uint32_t bytes) noexcept
    {
        bandwidthUsedThisTick += bytes;
    }
};

// ─── GlobalEntityVersionTable ─────────────────────────────────────────────────

/// Server-side source of truth for component version numbers.
/// Incremented whenever a component is written by simulation.
class GlobalEntityVersionTable final
{
public:
    GlobalEntityVersionTable();
    ~GlobalEntityVersionTable();

    GlobalEntityVersionTable(const GlobalEntityVersionTable&)            = delete;
    GlobalEntityVersionTable& operator=(const GlobalEntityVersionTable&) = delete;

    // ── Version mutation ──────────────────────────────────────────────────────

    /// Bump the version for a specific component. Returns the new version.
    ComponentVersion Increment(EntityId entityId, ComponentTypeId typeId);

    /// Retrieve current version without modification.
    [[nodiscard]] ComponentVersion Get(EntityId entityId, ComponentTypeId typeId) const;

    /// Remove all version tracking for an entity (entity destroyed).
    void RemoveEntity(EntityId entityId);

    /// Remove a specific component's version tracking.
    void RemoveComponent(EntityId entityId, ComponentTypeId typeId);

    // ── Queries ───────────────────────────────────────────────────────────────

    [[nodiscard]] bool HasEntity(EntityId entityId) const;

    /// Collect all entities with any dirty component since lastCheckedSequence.
    /// Used by ReplicationManager to build the dirty set without full iteration.
    [[nodiscard]] std::vector<EntityId> GetDirtyEntities() const;

    void ClearDirtySet();

private:
    struct Row
    {
        std::array<ComponentVersion, kMaxComponentTypes> versions;
        Row() { versions.fill(0); }
    };

    mutable std::mutex                          m_mutex;
    std::unordered_map<EntityId, Row>           m_table;
    std::unordered_map<EntityId, uint64_t>      m_dirtySet; ///< entityId → last modified tick
};

// ─── ReplicationStateManager ──────────────────────────────────────────────────

/// Maintains GlobalEntityVersionTable and all ClientReplicationRecords.
/// This is the single authoritative source for "what does each client know?"
class ReplicationStateManager final
{
public:
    ReplicationStateManager();
    ~ReplicationStateManager();

    ReplicationStateManager(const ReplicationStateManager&)            = delete;
    ReplicationStateManager& operator=(const ReplicationStateManager&) = delete;

    // ── Client management ─────────────────────────────────────────────────────

    void AddClient(ClientId clientId, uint32_t bandwidthBudgetBytesPerTick = 0);
    void RemoveClient(ClientId clientId);
    [[nodiscard]] bool HasClient(ClientId clientId) const;

    // ── Entity management ─────────────────────────────────────────────────────

    void RegisterEntity(EntityId entityId);
    void UnregisterEntity(EntityId entityId);

    // ── Dirty marking ─────────────────────────────────────────────────────────

    /// Mark a component dirty and bump its global version.
    /// Must be called from the simulation thread after each write.
    void MarkDirty(EntityId entityId, ComponentTypeId typeId);

    /// Bulk-mark all components of an entity dirty (e.g. on entity creation).
    void MarkAllDirty(EntityId entityId);

    // ── Snapshot building ─────────────────────────────────────────────────────

    /// Advance the sequence counter for all clients and collect dirty entity sets.
    /// Called once per replication interval from the tick thread.
    void BeginReplicationPass();

    /// Collect the list of entities that need updates for a specific client.
    /// Respects bandwidth budget and interest filter.
    [[nodiscard]] std::vector<EntityId>
        CollectDirtyForClient(ClientId clientId,
                              const std::vector<EntityId>& interestSet) const;

    /// Record that component data was sent to a client at the current sequence.
    void OnComponentSent(ClientId        clientId,
                         EntityId        entityId,
                         ComponentTypeId typeId);

    /// Record that a client has acknowledged receipt up to sequence seq.
    void OnClientAck(ClientId clientId, uint64_t ackSequence);

    // ── Queries ───────────────────────────────────────────────────────────────

    [[nodiscard]] ComponentVersion
        GetComponentVersion(EntityId entityId, ComponentTypeId typeId) const;

    [[nodiscard]] const ClientReplicationRecord*
        GetClientRecord(ClientId clientId) const;

    [[nodiscard]] uint64_t GetCurrentSequence() const noexcept;

private:
    mutable std::mutex                                         m_mutex;
    GlobalEntityVersionTable                                   m_versionTable;
    std::unordered_map<ClientId, ClientReplicationRecord>     m_clientRecords;
    std::atomic<uint64_t>                                     m_sequence{0};
};

} // namespace SagaEngine::Networking::Replication
