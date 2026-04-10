/// @file ReplicationState.cpp
/// @brief GlobalEntityVersionTable and ReplicationStateManager implementations.

#include "SagaServer/Networking/Replication/ReplicationState.h"
#include "SagaEngine/Core/Log/Log.h"

#include <algorithm>
#include <cassert>

namespace SagaEngine::Networking::Replication
{

static constexpr const char* kTag = "ReplicationState";

// ─── GlobalEntityVersionTable ─────────────────────────────────────────────────

GlobalEntityVersionTable::GlobalEntityVersionTable() = default;
GlobalEntityVersionTable::~GlobalEntityVersionTable() = default;

ComponentVersion GlobalEntityVersionTable::Increment(EntityId entityId, ComponentTypeId typeId)
{
    if (typeId >= kMaxComponentTypes)
    {
        LOG_WARN(kTag, "ComponentTypeId %u exceeds kMaxComponentTypes — ignoring",
                 static_cast<uint32_t>(typeId));
        return 0;
    }

    std::lock_guard lock(m_mutex);

    auto& row = m_table[entityId];
    const ComponentVersion newVer = ++row.versions[typeId];
    m_dirtySet[entityId] = m_dirtySet[entityId]; // ensure key exists
    return newVer;
}

ComponentVersion GlobalEntityVersionTable::Get(EntityId entityId, ComponentTypeId typeId) const
{
    if (typeId >= kMaxComponentTypes)
        return 0;

    std::lock_guard lock(m_mutex);
    const auto it = m_table.find(entityId);
    if (it == m_table.end())
        return 0;
    return it->second.versions[typeId];
}

void GlobalEntityVersionTable::RemoveEntity(EntityId entityId)
{
    std::lock_guard lock(m_mutex);
    m_table.erase(entityId);
    m_dirtySet.erase(entityId);
}

void GlobalEntityVersionTable::RemoveComponent(EntityId entityId, ComponentTypeId typeId)
{
    if (typeId >= kMaxComponentTypes)
        return;

    std::lock_guard lock(m_mutex);
    const auto it = m_table.find(entityId);
    if (it == m_table.end())
        return;

    it->second.versions[typeId] = 0;
}

bool GlobalEntityVersionTable::HasEntity(EntityId entityId) const
{
    std::lock_guard lock(m_mutex);
    return m_table.count(entityId) > 0;
}

std::vector<EntityId> GlobalEntityVersionTable::GetDirtyEntities() const
{
    std::lock_guard lock(m_mutex);
    std::vector<EntityId> result;
    result.reserve(m_dirtySet.size());
    for (const auto& [eid, _] : m_dirtySet)
        result.push_back(eid);
    return result;
}

void GlobalEntityVersionTable::ClearDirtySet()
{
    std::lock_guard lock(m_mutex);
    m_dirtySet.clear();
}

// ─── ReplicationStateManager ──────────────────────────────────────────────────

ReplicationStateManager::ReplicationStateManager() = default;
ReplicationStateManager::~ReplicationStateManager() = default;

// ─── Client management ────────────────────────────────────────────────────────

void ReplicationStateManager::AddClient(ClientId clientId,
                                         uint32_t bandwidthBudgetBytesPerTick)
{
    std::lock_guard lock(m_mutex);

    if (m_clientRecords.count(clientId))
    {
        LOG_WARN(kTag, "Client %llu already registered — skipping AddClient",
                 static_cast<unsigned long long>(clientId));
        return;
    }

    ClientReplicationRecord rec;
    rec.clientId                = clientId;
    rec.bandwidthBudgetPerTick  = bandwidthBudgetBytesPerTick;
    rec.currentSequence         = m_sequence.load(std::memory_order_relaxed);
    rec.lastAckedSequence       = 0;

    m_clientRecords[clientId] = std::move(rec);

    LOG_DEBUG(kTag, "Client %llu added — bandwidth budget %u bytes/tick",
              static_cast<unsigned long long>(clientId),
              bandwidthBudgetBytesPerTick);
}

void ReplicationStateManager::RemoveClient(ClientId clientId)
{
    std::lock_guard lock(m_mutex);
    const std::size_t n = m_clientRecords.erase(clientId);
    if (n == 0)
        LOG_WARN(kTag, "RemoveClient: client %llu not found",
                 static_cast<unsigned long long>(clientId));
}

bool ReplicationStateManager::HasClient(ClientId clientId) const
{
    std::lock_guard lock(m_mutex);
    return m_clientRecords.count(clientId) > 0;
}

// ─── Entity management ────────────────────────────────────────────────────────

void ReplicationStateManager::RegisterEntity(EntityId entityId)
{
    // Presence in m_versionTable is lazy; ensure all clients get an isNew=true state.
    std::lock_guard lock(m_mutex);
    for (auto& [cid, rec] : m_clientRecords)
    {
        auto& state = rec.GetOrCreate(entityId);
        state.isNew = true;
    }
}

void ReplicationStateManager::UnregisterEntity(EntityId entityId)
{
    std::lock_guard lock(m_mutex);
    m_versionTable.RemoveEntity(entityId);
    for (auto& [cid, rec] : m_clientRecords)
        rec.RemoveEntity(entityId);
}

// ─── Dirty marking ────────────────────────────────────────────────────────────

void ReplicationStateManager::MarkDirty(EntityId entityId, ComponentTypeId typeId)
{
    const ComponentVersion newVer = m_versionTable.Increment(entityId, typeId);

    std::lock_guard lock(m_mutex);
    for (auto& [cid, rec] : m_clientRecords)
    {
        auto& state = rec.GetOrCreate(entityId);
        state.MarkDirty(typeId, newVer);
    }
}

void ReplicationStateManager::MarkAllDirty(EntityId entityId)
{
    std::lock_guard lock(m_mutex);
    for (auto& [cid, rec] : m_clientRecords)
    {
        auto& state = rec.GetOrCreate(entityId);
        for (uint32_t i = 0; i < kMaxComponentTypes; ++i)
        {
            const ComponentVersion v =
                m_versionTable.Get(entityId, static_cast<ComponentTypeId>(i));
            if (v > 0)
                state.MarkDirty(static_cast<ComponentTypeId>(i), v);
        }
        state.isNew = true;
    }
}

// ─── Snapshot building ────────────────────────────────────────────────────────

void ReplicationStateManager::BeginReplicationPass()
{
    const uint64_t seq = m_sequence.fetch_add(1, std::memory_order_relaxed) + 1;

    std::lock_guard lock(m_mutex);
    for (auto& [cid, rec] : m_clientRecords)
    {
        rec.currentSequence = seq;
        rec.ResetBandwidthAccumulator();
    }

    m_versionTable.ClearDirtySet();
}

std::vector<EntityId> ReplicationStateManager::CollectDirtyForClient(
    ClientId                     clientId,
    const std::vector<EntityId>& interestSet) const
{
    std::lock_guard lock(m_mutex);

    const auto recIt = m_clientRecords.find(clientId);
    if (recIt == m_clientRecords.end())
        return {};

    const ClientReplicationRecord& rec = recIt->second;

    std::vector<EntityId> result;
    result.reserve(std::min(interestSet.size(), std::size_t{256}));

    for (EntityId entityId : interestSet)
    {
        const auto stateIt = rec.entityStates.find(entityId);
        if (stateIt == rec.entityStates.end())
            continue;

        if (stateIt->second.NeedsUpdate())
            result.push_back(entityId);
    }

    return result;
}

void ReplicationStateManager::OnComponentSent(ClientId        clientId,
                                               EntityId        entityId,
                                               ComponentTypeId typeId)
{
    std::lock_guard lock(m_mutex);

    const auto recIt = m_clientRecords.find(clientId);
    if (recIt == m_clientRecords.end())
        return;

    ClientReplicationRecord& rec = recIt->second;
    auto stateIt = rec.entityStates.find(entityId);
    if (stateIt == rec.entityStates.end())
        return;

    stateIt->second.OnSent(typeId, rec.currentSequence);
}

void ReplicationStateManager::OnClientAck(ClientId clientId, uint64_t ackSequence)
{
    std::lock_guard lock(m_mutex);

    const auto recIt = m_clientRecords.find(clientId);
    if (recIt == m_clientRecords.end())
        return;

    ClientReplicationRecord& rec = recIt->second;

    if (ackSequence <= rec.lastAckedSequence)
        return;

    rec.lastAckedSequence = ackSequence;

    for (auto& [eid, state] : rec.entityStates)
        state.OnAck(ackSequence);
}

// ─── Queries ──────────────────────────────────────────────────────────────────

ComponentVersion ReplicationStateManager::GetComponentVersion(
    EntityId        entityId,
    ComponentTypeId typeId) const
{
    return m_versionTable.Get(entityId, typeId);
}

const ClientReplicationRecord* ReplicationStateManager::GetClientRecord(ClientId clientId) const
{
    std::lock_guard lock(m_mutex);
    const auto it = m_clientRecords.find(clientId);
    if (it == m_clientRecords.end())
        return nullptr;
    return &it->second;
}

uint64_t ReplicationStateManager::GetCurrentSequence() const noexcept
{
    return m_sequence.load(std::memory_order_relaxed);
}

} // namespace SagaEngine::Networking::Replication
