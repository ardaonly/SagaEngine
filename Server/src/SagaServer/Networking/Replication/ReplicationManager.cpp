#include "SagaServer/Networking/Replication/ReplicationManager.h"
#include "SagaEngine/Simulation/Authority.h"
#include "SagaEngine/Simulation/WorldState.h"
#include "SagaEngine/Core/Log/Log.h"
#include <algorithm>

namespace SagaEngine::Networking::Replication {

struct ReplicationManager::Impl {
    std::unordered_map<ClientId, std::unique_ptr<ClientReplicationState>> clientStates;
    std::unordered_map<uint32_t, RPCDefinition>                           rpcRegistry;
    std::unordered_map<EntityId, std::vector<ComponentTypeId>>            dirtyComponents;
    std::vector<EntityId>                                                  registeredEntities;

    IWorldSnapshot*                  worldSnapshot{nullptr};
    Simulation::AuthorityManager*    authorityManager{nullptr};
    Simulation::WorldState*          worldState{nullptr};
    Interest::InterestManager*       interestManager{nullptr};
    bool                             isShutdown{false};
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
        LOG_WARN("Replication", "Client %llu already registered", clientId);
        return;
    }

    auto state       = std::make_unique<ClientReplicationState>();
    state->clientId  = clientId;

    for (EntityId entityId : _pimpl->registeredEntities)
        state->priorities[entityId] = {};

    _pimpl->clientStates[clientId] = std::move(state);
    LOG_INFO("Replication", "Client %llu added", clientId);
}

void ReplicationManager::RemoveClient(ClientId clientId)
{
    std::lock_guard<std::mutex> lock(_stateMutex);
    _pimpl->clientStates.erase(clientId);
    LOG_INFO("Replication", "Client %llu removed", clientId);
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

    auto it = std::find(_pimpl->registeredEntities.begin(),
                        _pimpl->registeredEntities.end(), entityId);
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
        if (!statePtr) continue;
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
        if (componentTypes.empty()) continue;

        for (auto& [clientId, statePtr] : _pimpl->clientStates)
        {
            if (!statePtr) continue;
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

void ReplicationManager::SendUpdates(ClientId clientId,
                                      std::function<void(const uint8_t*, size_t)> sendCallback)
{
    if (_pimpl->isShutdown) return;

    std::vector<PendingUpdate> work;
    {
        std::lock_guard<std::mutex> lock(_stateMutex);

        auto it = _pimpl->clientStates.find(clientId);
        if (it == _pimpl->clientStates.end())
        {
            LOG_DEBUG("Replication", "SendUpdates: unknown client %llu", clientId);
            return;
        }
        if (!it->second || !it->second->isDirty)
            return;

        work = CollectPendingUpdates(clientId);
    }

    if (work.empty()) return;

    IWorldSnapshot* snapshot = _pimpl->worldSnapshot;

    Packet updatePacket(PacketType::DeltaSnapshot);
    size_t entitiesSent = 0;

    for (const auto& upd : work)
    {
        if (updatePacket.GetTotalSize() > 1200)
        {
            LOG_DEBUG("Replication", "MTU limit reached for client %llu", clientId);
            break;
        }

        updatePacket.Write(upd.entityId);
        uint16_t compCount = static_cast<uint16_t>(upd.components.size());
        updatePacket.Write(compCount);

        for (ComponentTypeId compType : upd.components)
        {
            updatePacket.Write(compType);

            const ECS::Serializer* ser =
                ECS::ComponentRegistry::Instance().GetSerializer(compType);
            if (!ser)
            {
                LOG_WARN("Replication", "No serializer for component %u", compType);
                uint32_t zero = 0;
                updatePacket.Write(zero);
                continue;
            }

            size_t bytes = 0;
            const size_t maxCompSize = 512;
            uint8_t compBuf[maxCompSize]{};

            if (snapshot)
            {
                bytes = snapshot->GetSerializedComponent(
                    upd.entityId, compType, compBuf, maxCompSize);
            }
            else if (_pimpl->worldState)
            {
                bytes = _pimpl->worldState->GetSerializedComponent(
                    upd.entityId, compType, compBuf, maxCompSize);
            }

            uint32_t serializedSize = static_cast<uint32_t>(bytes);
            updatePacket.Write(serializedSize);

            if (bytes > 0)
            {
                updatePacket.WriteBytes(compBuf, bytes);
            }
            std::lock_guard<std::mutex> lock(_stateMutex);
            auto stateIt = _pimpl->clientStates.find(clientId);
            if (stateIt != _pimpl->clientStates.end() && stateIt->second)
            {
                auto& sent = stateIt->second->sentComponents[upd.entityId];
                for (ComponentTypeId compType : upd.components)
                    sent.insert(compType);
            }
        }

        entitiesSent++;
    }

    if (entitiesSent > 0)
    {
        auto data = updatePacket.GetSerializedData();
        sendCallback(data.data(), data.size());

        LOG_DEBUG("Replication", "Sent %zu entities to client %llu (%zu bytes)",
                  entitiesSent, clientId, data.size());
    }
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
            LOG_WARN("RPC", "Unknown RPC %u from client %llu", rpcId, clientId);
            return;
        }
        def = it->second;
    }

    if (def.requiresEntityAuthority && !CheckEntityAuthority(clientId, targetEntity))
    {
        LOG_WARN("RPC", "Authority denied: client %llu, entity %u, RPC %u",
                 clientId, targetEntity, rpcId);
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
    if (!_pimpl->authorityManager) return true;
    return _pimpl->authorityManager->CanModify(static_cast<uint32_t>(entityId));
}

void ReplicationManager::ProcessEntityDelta(ClientReplicationState& clientState,
                                            EntityId entityId)
{
    auto& sent = clientState.sentComponents[entityId];

    if (!_pimpl->worldSnapshot && !_pimpl->worldState)
        return;

    const ECS::ComponentRegistry& registry = ECS::ComponentRegistry::Instance();
    size_t componentCount = registry.GetComponentCount();

    for (ECS::ComponentTypeId typeId = 0;
         typeId < static_cast<ECS::ComponentTypeId>(componentCount);
         ++typeId)
    {
        bool hasComponent = false;

        if (_pimpl->worldSnapshot)
            hasComponent = _pimpl->worldSnapshot->EntityHasComponent(entityId, typeId);
        else if (_pimpl->worldState)
            hasComponent = _pimpl->worldState->EntityHasComponent(entityId, typeId);

        if (!hasComponent)
            continue;

        if (sent.count(typeId) == 0)
        {
            auto& pending = clientState.pendingComponents[entityId];
            if (std::find(pending.begin(), pending.end(), typeId) == pending.end())
                pending.push_back(typeId);
        }
    }
}

} // namespace SagaEngine::Networking::Replication