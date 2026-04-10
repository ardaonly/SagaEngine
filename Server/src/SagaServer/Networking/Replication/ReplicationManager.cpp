/// @file ReplicationManager.cpp
/// @brief Server-side replication manager implementation.

#include "SagaServer/Networking/Replication/ReplicationManager.h"
#include "SagaServer/Networking/Interest/InterestArea.h"
#include "SagaEngine/Simulation/Authority.h"
#include "SagaEngine/Simulation/WorldState.h"
#include "SagaEngine/Core/Log/Log.h"

#include <algorithm>
#include <limits>

namespace SagaEngine::Networking::Replication
{

struct ReplicationManager::Impl
{
    std::unordered_map<ClientId, std::unique_ptr<ClientReplicationState>> clientStates;
    std::unordered_map<uint32_t, RPCDefinition>                           rpcRegistry;
    std::unordered_map<EntityId, std::vector<ComponentTypeId>>            dirtyComponents;
    std::vector<EntityId>                                                 registeredEntities;

    IWorldSnapshot*                    worldSnapshot{nullptr};
    Simulation::AuthorityManager*      authorityManager{nullptr};
    Simulation::WorldState*            worldState{nullptr};
    Interest::InterestManager*         interestManager{nullptr};
    bool                               isShutdown{false};
};

ReplicationManager::ReplicationManager()
    : _pimpl(std::make_unique<Impl>())
{
}

ReplicationManager::~ReplicationManager()
{
    Shutdown();
}

void ReplicationManager::Initialize(uint32_t maxClients)
{
    std::lock_guard<std::mutex> lock(_stateMutex);
    _pimpl->isShutdown = false;
    _pimpl->clientStates.reserve(maxClients);
    LOG_INFO("Replication", "Initialized (maxClients=%u)", maxClients);
}

void ReplicationManager::Shutdown()
{
    std::lock_guard<std::mutex> lock(_stateMutex);
    _pimpl->isShutdown = true;
    _pimpl->clientStates.clear();
    _pimpl->dirtyComponents.clear();
    _pimpl->registeredEntities.clear();
    _pimpl->rpcRegistry.clear();
    LOG_INFO("Replication", "Shutdown complete");
}

void ReplicationManager::AddClient(ClientId clientId)
{
    std::lock_guard<std::mutex> lock(_stateMutex);

    if (_pimpl->clientStates.count(clientId))
    {
        LOG_WARN("Replication", "Client %llu already registered",
                 static_cast<unsigned long long>(clientId));
        return;
    }

    auto state = std::make_unique<ClientReplicationState>();
    state->clientId = clientId;

    for (EntityId entityId : _pimpl->registeredEntities)
        state->priorities[entityId] = {};

    _pimpl->clientStates[clientId] = std::move(state);
    LOG_INFO("Replication", "Client %llu added",
             static_cast<unsigned long long>(clientId));
}

void ReplicationManager::RemoveClient(ClientId clientId)
{
    std::lock_guard<std::mutex> lock(_stateMutex);
    _pimpl->clientStates.erase(clientId);
    LOG_INFO("Replication", "Client %llu removed",
             static_cast<unsigned long long>(clientId));
}

bool ReplicationManager::HasClient(ClientId clientId) const
{
    std::lock_guard<std::mutex> lock(_stateMutex);
    return _pimpl->clientStates.count(clientId) > 0;
}

void ReplicationManager::SetWorldState(Simulation::WorldState* world)
{
    _pimpl->worldState = world;
}

void ReplicationManager::SetWorldSnapshot(IWorldSnapshot* snapshot)
{
    _pimpl->worldSnapshot = snapshot;
}

void ReplicationManager::SetAuthorityManager(Simulation::AuthorityManager* auth)
{
    _pimpl->authorityManager = auth;
}

void ReplicationManager::SetInterestManager(Interest::InterestManager* interest)
{
    _pimpl->interestManager = interest;
}

void ReplicationManager::SetReplicationFrequency(float hz)
{
    _replicationHz = hz;
}

void ReplicationManager::RegisterEntityForReplication(EntityId entityId)
{
    std::lock_guard<std::mutex> lock(_stateMutex);

    const auto it = std::find(_pimpl->registeredEntities.begin(),
                              _pimpl->registeredEntities.end(),
                              entityId);
    if (it != _pimpl->registeredEntities.end())
        return;

    _pimpl->registeredEntities.push_back(entityId);

    for (auto& [cid, statePtr] : _pimpl->clientStates)
    {
        if (statePtr)
            statePtr->priorities[entityId] = {};
    }
}

void ReplicationManager::UnregisterEntity(EntityId entityId)
{
    std::lock_guard<std::mutex> lock(_stateMutex);

    auto& reg = _pimpl->registeredEntities;
    reg.erase(std::remove(reg.begin(), reg.end(), entityId), reg.end());
    _pimpl->dirtyComponents.erase(entityId);

    for (auto& [cid, statePtr] : _pimpl->clientStates)
    {
        if (!statePtr)
            continue;

        statePtr->sentComponents.erase(entityId);
        statePtr->pendingComponents.erase(entityId);
        statePtr->priorities.erase(entityId);
    }
}

void ReplicationManager::MarkComponentDirty(EntityId entityId, ComponentTypeId componentId)
{
    std::lock_guard<std::mutex> lock(_stateMutex);

    auto& components = _pimpl->dirtyComponents[entityId];
    if (std::find(components.begin(), components.end(), componentId) == components.end())
        components.push_back(componentId);
}

void ReplicationManager::GatherDirtyEntities(float dt, bool force)
{
    if (!force)
    {
        _accumulator += dt;
        if (_accumulator < (1.0f / _replicationHz))
            return;
        _accumulator = 0.0f;
    }

    std::lock_guard<std::mutex> lock(_stateMutex);

    for (const auto& [entityId, componentTypes] : _pimpl->dirtyComponents)
    {
        if (componentTypes.empty())
            continue;

        for (auto& [clientId, statePtr] : _pimpl->clientStates)
        {
            if (!statePtr)
                continue;

            auto& state = *statePtr;

            ProcessEntityDelta(state, entityId);

            auto& pending = state.pendingComponents[entityId];
            for (ComponentTypeId compType : componentTypes)
            {
                if (std::find(pending.begin(), pending.end(), compType) == pending.end())
                    pending.push_back(compType);
            }

            state.isDirty = true;
        }
    }

    _pimpl->dirtyComponents.clear();
}

std::vector<ReplicationManager::PendingUpdate>
ReplicationManager::CollectPendingUpdates(ClientId clientId)
{
    std::vector<PendingUpdate> work;

    auto it = _pimpl->clientStates.find(clientId);
    if (it == _pimpl->clientStates.end() || !it->second || !it->second->isDirty)
        return work;

    ClientReplicationState& state = *it->second;

    // ── Build candidate set ──────────────────────────────────────────────────
    //
    // When an interest manager is wired in we honour its culling, otherwise we
    // fall back to every entity that currently has pending components. The
    // scoring stage below reorders the candidate list so critical updates win
    // when bandwidth runs short.

    std::vector<EntityId> targetEntities;

    if (_pimpl->interestManager)
    {
        targetEntities = _pimpl->interestManager->GetVisibleEntities(clientId);
    }
    else
    {
        targetEntities.reserve(state.pendingComponents.size());
        for (const auto& [eid, comps] : state.pendingComponents)
        {
            if (!comps.empty())
                targetEntities.push_back(eid);
        }
    }

    // ── Priority ordering ────────────────────────────────────────────────────
    //
    // Use ReplicationPriority::CalculateScore() to sort candidates descending.
    // Entities without a stored priority slot default to zero so they sink to
    // the bottom of the queue.

    std::sort(targetEntities.begin(), targetEntities.end(),
              [&state](EntityId a, EntityId b)
              {
                  const auto itA = state.priorities.find(a);
                  const auto itB = state.priorities.find(b);

                  const float scoreA = (itA != state.priorities.end())
                                       ? itA->second.CalculateScore() : 0.0f;
                  const float scoreB = (itB != state.priorities.end())
                                       ? itB->second.CalculateScore() : 0.0f;

                  return scoreA > scoreB;
              });

    for (EntityId entityId : targetEntities)
    {
        auto pendingIt = state.pendingComponents.find(entityId);
        if (pendingIt == state.pendingComponents.end() || pendingIt->second.empty())
            continue;

        PendingUpdate upd;
        upd.entityId   = entityId;
        upd.components = pendingIt->second;
        work.push_back(std::move(upd));

        state.pendingComponents.erase(pendingIt);
    }

    state.isDirty = !state.pendingComponents.empty();
    return work;
}

void ReplicationManager::SendUpdates(
    ClientId clientId,
    std::function<void(const uint8_t*, size_t)> sendCallback)
{
    if (_pimpl->isShutdown)
        return;

    std::vector<PendingUpdate> work;
    {
        std::lock_guard<std::mutex> lock(_stateMutex);

        auto it = _pimpl->clientStates.find(clientId);
        if (it == _pimpl->clientStates.end())
        {
            LOG_DEBUG("Replication", "SendUpdates: unknown client %llu",
                      static_cast<unsigned long long>(clientId));
            return;
        }

        if (!it->second || !it->second->isDirty)
            return;

        work = CollectPendingUpdates(clientId);
    }

    if (work.empty())
        return;

    Packet updatePacket(PacketType::DeltaSnapshot);
    updatePacket.Write(static_cast<uint32_t>(work.size()));

    for (const auto& upd : work)
    {
        if (updatePacket.GetTotalSize() > 1200)
        {
            LOG_DEBUG("Replication", "MTU limit reached for client %llu",
                      static_cast<unsigned long long>(clientId));
            break;
        }

        updatePacket.Write(upd.entityId);
        updatePacket.Write(static_cast<uint16_t>(upd.components.size()));

        for (ComponentTypeId compType : upd.components)
            updatePacket.Write(compType);
    }

    if (_pimpl->worldState)
    {
        const std::vector<uint8_t> snapshotBytes = _pimpl->worldState->Serialize();
        updatePacket.Write(static_cast<uint32_t>(snapshotBytes.size()));
        if (!snapshotBytes.empty())
            updatePacket.WriteBytes(snapshotBytes.data(), snapshotBytes.size());
    }
    else if (_pimpl->worldSnapshot)
    {
        const std::vector<EntityId> entities = _pimpl->worldSnapshot->GetAllEntities();
        updatePacket.Write(static_cast<uint32_t>(entities.size()));
        for (EntityId entityId : entities)
            updatePacket.Write(entityId);
    }

    const auto data = updatePacket.GetSerializedData();
    sendCallback(data.data(), data.size());

    {
        std::lock_guard<std::mutex> lock(_stateMutex);
        auto stateIt = _pimpl->clientStates.find(clientId);
        if (stateIt != _pimpl->clientStates.end() && stateIt->second)
        {
            for (const auto& upd : work)
            {
                auto& sent = stateIt->second->sentComponents[upd.entityId];
                for (ComponentTypeId compType : upd.components)
                    sent.insert(compType);
            }
        }
    }

    LOG_DEBUG("Replication", "Sent %zu entities to client %llu (%zu bytes)",
              work.size(),
              static_cast<unsigned long long>(clientId),
              data.size());
}

void ReplicationManager::ReceiveRPC(ClientId       clientId,
                                    uint32_t       rpcId,
                                    EntityId       targetEntity,
                                    const uint8_t* payload,
                                    size_t         size)
{
    RPCDefinition def;
    {
        std::lock_guard<std::mutex> lock(_rpcMutex);

        auto it = _pimpl->rpcRegistry.find(rpcId);
        if (it == _pimpl->rpcRegistry.end())
        {
            LOG_WARN("RPC", "Unknown RPC %u from client %llu",
                     rpcId, static_cast<unsigned long long>(clientId));
            return;
        }

        def = it->second;
    }

    if (def.requiresEntityAuthority && !CheckEntityAuthority(clientId, targetEntity))
    {
        LOG_WARN("RPC", "Authority denied: client %llu, entity %u, RPC %u",
                 static_cast<unsigned long long>(clientId),
                 static_cast<unsigned int>(targetEntity),
                 rpcId);
        return;
    }

    try
    {
        def.serverHandler(clientId, targetEntity, payload, size);
    }
    catch (const std::exception& e)
    {
        LOG_ERROR("RPC", "Handler exception for RPC %u: %s", rpcId, e.what());
    }
}

void ReplicationManager::RegisterRPCInternal(const RPCDefinition& def)
{
    std::lock_guard<std::mutex> lock(_rpcMutex);
    _pimpl->rpcRegistry[def.id] = def;
    LOG_INFO("RPC", "Registered '%s' (id=%u, entityAuth=%s)",
             def.name.c_str(), def.id,
             def.requiresEntityAuthority ? "yes" : "no");
}

bool ReplicationManager::CheckEntityAuthority(ClientId clientId, EntityId entityId) const
{
    (void)clientId;

    if (!_pimpl->authorityManager)
        return true;

    return _pimpl->authorityManager->CanModify(static_cast<uint32_t>(entityId));
}

void ReplicationManager::ProcessEntityDelta(ClientReplicationState& clientState,
                                            EntityId entityId)
{
    // ── Visibility gate ───────────────────────────────────────────────────────
    //
    // If an InterestManager is wired in, an entity that has slipped out of the
    // client's view does not belong in its pending queue or in its sent record.
    // Dropping it here prevents stale deltas from accumulating while the entity
    // stays out of range and lets the tombstone path be driven by unregistration.

    bool isVisible = true;

    if (_pimpl->interestManager)
    {
        const Interest::AreaId ownerArea = _pimpl->interestManager->GetEntityArea(entityId);
        if (ownerArea == 0)
        {
            isVisible = false;
        }
        else
        {
            const auto subscribed =
                _pimpl->interestManager->GetClientSubscribedAreas(clientState.clientId);
            isVisible = std::find(subscribed.begin(), subscribed.end(), ownerArea)
                        != subscribed.end();
        }
    }

    if (!isVisible)
    {
        clientState.pendingComponents.erase(entityId);
        clientState.sentComponents.erase(entityId);
        clientState.priorities.erase(entityId);
        return;
    }

    // ── Priority bookkeeping ──────────────────────────────────────────────────
    //
    // Ensure a ReplicationPriority slot exists for this pair and refresh its
    // distance/importance scores. Distance is only meaningful when the world
    // state can expose an entity position — if it cannot we fall back to a
    // neutral score so CalculateScore() remains well-defined.

    ReplicationPriority& priority = clientState.priorities[entityId];

    if (priority.lastReplicatedTick == 0)
        priority.importanceScore = 1.0f;

    if (_pimpl->interestManager)
    {
        const auto visible = _pimpl->interestManager->GetVisibleEntities(clientState.clientId);
        const bool stillVisible =
            std::find(visible.begin(), visible.end(), entityId) != visible.end();

        if (!stillVisible)
        {
            priority.distanceScore = std::numeric_limits<float>::infinity();
        }
        else if (priority.distanceScore <= 0.0f)
        {
            priority.distanceScore = 1.0f;
        }
    }

    // ── Sent-set reconciliation ───────────────────────────────────────────────
    //
    // An entity that no longer appears in the global registry has been destroyed
    // on the authoritative side. Drop it from the client's sent record so the
    // next replication pass can emit a tombstone without tripping the dedup
    // logic further down the pipeline.

    const bool stillRegistered =
        std::find(_pimpl->registeredEntities.begin(),
                  _pimpl->registeredEntities.end(),
                  entityId) != _pimpl->registeredEntities.end();

    if (!stillRegistered)
    {
        clientState.sentComponents.erase(entityId);
        clientState.pendingComponents.erase(entityId);
        clientState.priorities.erase(entityId);
        return;
    }

    // ── Pending queue dedup ───────────────────────────────────────────────────
    //
    // Entries in pendingComponents that were never acknowledged but have since
    // been re-dirtied must not be duplicated. We sort and unique in place so the
    // caller can freely append without quadratic search later in the hot path.

    auto pendingIt = clientState.pendingComponents.find(entityId);
    if (pendingIt != clientState.pendingComponents.end())
    {
        auto& comps = pendingIt->second;
        std::sort(comps.begin(), comps.end());
        comps.erase(std::unique(comps.begin(), comps.end()), comps.end());
    }

    clientState.isDirty = true;
}

} // namespace SagaEngine::Networking::Replication