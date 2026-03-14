#include "SagaEngine/Networking/Replication/ReplicationManager.h"
#include "SagaEngine/Simulation/WorldState.h"
#include "SagaEngine/Simulation/Authority.h"
#include "SagaEngine/Core/Log/Log.h"
#include <algorithm>
#include <cstring>

namespace SagaEngine::Networking::Replication {

struct ReplicationManager::Impl {
    std::unordered_map<ClientId, std::unique_ptr<ClientReplicationState>> clientStates;
    std::unordered_map<uint32_t, RPCDefinition> rpcRegistry;
    std::unordered_map<EntityId, std::vector<ComponentTypeId>> dirtyComponents;
    std::vector<EntityId> registeredEntities;
    
    Simulation::WorldState* worldState{nullptr};
    Simulation::AuthorityManager* authorityManager{nullptr};
    Interest::InterestManager* interestManager{nullptr};  // ✅ Interest Management
};

ReplicationManager::ReplicationManager() 
    : _pimpl(std::make_unique<Impl>()) 
{
}

ReplicationManager::~ReplicationManager() {
    Shutdown();
}

void ReplicationManager::Initialize(uint32_t maxClients) {
    LOG_INFO("Replication", "Initializing (maxClients=%u)", maxClients);
    _pimpl->clientStates.reserve(maxClients);
}

void ReplicationManager::Shutdown() {
    std::lock_guard<std::mutex> lock(_stateMutex);
    _pimpl->clientStates.clear();
    _pimpl->dirtyComponents.clear();
    _pimpl->registeredEntities.clear();
    _pimpl->rpcRegistry.clear();
    LOG_INFO("Replication", "Shutdown complete");
}

void ReplicationManager::AddClient(ClientId clientId) {
    std::lock_guard<std::mutex> lock(_stateMutex);
    
    if (_pimpl->clientStates.find(clientId) != _pimpl->clientStates.end()) {
        LOG_WARN("Replication", "Client %llu already exists", clientId);
        return;
    }
    
    auto state = std::make_unique<ClientReplicationState>();
    state->clientId = clientId;
    
    for (EntityId entityId : _pimpl->registeredEntities) {
        state->entityComponentMasks[entityId] = 0;
        state->priorities[entityId] = ReplicationPriority{};
    }
    
    _pimpl->clientStates[clientId] = std::move(state);
    LOG_INFO("Replication", "Client %llu added", clientId);
}

void ReplicationManager::RemoveClient(ClientId clientId) {
    std::lock_guard<std::mutex> lock(_stateMutex);
    _pimpl->clientStates.erase(clientId);
    LOG_INFO("Replication", "Client %llu removed", clientId);
}

bool ReplicationManager::HasClient(ClientId clientId) const {
    std::lock_guard<std::mutex> lock(_stateMutex);
    return _pimpl->clientStates.find(clientId) != _pimpl->clientStates.end();
}

void ReplicationManager::SetReplicationFrequency(float hz) {
    _replicationHz = hz;
}

void ReplicationManager::SetWorldState(Simulation::WorldState* world) {
    _pimpl->worldState = world;
}

void ReplicationManager::SetAuthorityManager(Simulation::AuthorityManager* auth) {
    _pimpl->authorityManager = auth;
}

void ReplicationManager::SetInterestManager(Interest::InterestManager* interest) {
    _pimpl->interestManager = interest;
}

void ReplicationManager::RegisterEntityForReplication(EntityId entityId) {
    std::lock_guard<std::mutex> lock(_stateMutex);
    
    auto it = std::find(_pimpl->registeredEntities.begin(), 
                        _pimpl->registeredEntities.end(), entityId);
    if (it == _pimpl->registeredEntities.end()) {
        _pimpl->registeredEntities.push_back(entityId);
        
        for (auto& [cid, statePtr] : _pimpl->clientStates) {
            if (statePtr) {
                statePtr->entityComponentMasks[entityId] = 0;
                statePtr->priorities[entityId] = ReplicationPriority{};
            }
        }
    }
}

void ReplicationManager::UnregisterEntity(EntityId entityId) {
    std::lock_guard<std::mutex> lock(_stateMutex);
    
    _pimpl->registeredEntities.erase(
        std::remove(_pimpl->registeredEntities.begin(), 
                    _pimpl->registeredEntities.end(), entityId),
        _pimpl->registeredEntities.end()
    );
    _pimpl->dirtyComponents.erase(entityId);
    
    for (auto& [cid, statePtr] : _pimpl->clientStates) {
        if (statePtr) {
            statePtr->entityComponentMasks.erase(entityId);
            statePtr->priorities.erase(entityId);
            statePtr->dirtyComponents.erase(entityId);
        }
    }
}

void ReplicationManager::MarkComponentDirty(EntityId entityId, ComponentTypeId componentId) {
    std::lock_guard<std::mutex> lock(_stateMutex);
    
    auto& components = _pimpl->dirtyComponents[entityId];
    auto it = std::find(components.begin(), components.end(), componentId);
    if (it == components.end()) {
        components.push_back(componentId);
    }
}

void ReplicationManager::GatherDirtyEntities(float dt, bool force) {
    if (!force) {
        _accumulator += dt;
        if (_accumulator < (1.0f / _replicationHz)) {
            return;
        }
        _accumulator = 0.0f;
    }

    std::lock_guard<std::mutex> lock(_stateMutex);
    
    for (const auto& [entityId, componentTypes] : _pimpl->dirtyComponents) {
        if (componentTypes.empty()) continue;
        
        for (auto& [clientId, statePtr] : _pimpl->clientStates) {
            if (!statePtr) continue;
            auto& state = *statePtr;
            
            if (state.priorities.find(entityId) == state.priorities.end()) {
                state.priorities[entityId] = ReplicationPriority{};
                state.entityComponentMasks[entityId] = 0;
            }
            
            auto& clientDirty = state.dirtyComponents[entityId];
            for (ComponentTypeId compType : componentTypes) {
                if (std::find(clientDirty.begin(), clientDirty.end(), compType) == clientDirty.end()) {
                    clientDirty.push_back(compType);
                }
            }
            
            state.isDirty = true;
        }
    }
    
    _pimpl->dirtyComponents.clear();
}

void ReplicationManager::SendUpdates(ClientId clientId, 
                                      std::function<void(const uint8_t*, size_t)> sendCallback) {
    std::lock_guard<std::mutex> lock(_stateMutex);
    
    auto it = _pimpl->clientStates.find(clientId);
    if (it == _pimpl->clientStates.end()) {
        LOG_DEBUG("Replication", "SendUpdates: Client %llu not found", clientId);
        return;
    }
    if (!it->second) {
        LOG_DEBUG("Replication", "SendUpdates: Client %llu state is null", clientId);
        return;
    }
    if (!it->second->isDirty) {
        LOG_DEBUG("Replication", "SendUpdates: Client %llu not dirty", clientId);
        return;
    }

    ClientReplicationState& state = *it->second;
    Packet updatePacket(PacketType::DeltaSnapshot);
    
    size_t entitiesSent = 0;
    
    std::vector<EntityId> targetEntities;
    
    if (_pimpl->interestManager) {
        targetEntities = _pimpl->interestManager->GetVisibleEntities(clientId);
    } else {
        for (const auto& [entityId, components] : state.dirtyComponents) {
            if (!components.empty()) {
                targetEntities.push_back(entityId);
            }
        }
    }
    
    for (EntityId entityId : targetEntities) {
        auto dirtyIt = state.dirtyComponents.find(entityId);
        if (dirtyIt == state.dirtyComponents.end()) continue;
        if (dirtyIt->second.empty()) continue;

        if (updatePacket.GetTotalSize() > 1200) {
            LOG_DEBUG("Replication", "Packet MTU limit reached for client %llu", clientId);
            break;
        }

        updatePacket.Write(entityId);
        uint16_t compCount = static_cast<uint16_t>(dirtyIt->second.size());
        updatePacket.Write(compCount);

        for (ComponentTypeId compType : dirtyIt->second) {
            updatePacket.Write(compType);
            
            const SagaEngine::ECS::Serializer* serializer = 
                SagaEngine::ECS::ComponentRegistry::Instance().GetSerializer(compType);
            if (!serializer) {
                LOG_WARN("Replication", "No serializer for component type %u", compType);
                continue;
            }
            if (!_pimpl->worldState) {
                LOG_WARN("Replication", "WorldState is null, skipping serialization");
                continue;
            }
            
            uint8_t dummyData[64] = {0};
            size_t size = serializer->GetSerializedSize();
            if (size <= 64) {
                updatePacket.WriteBytes(dummyData, size);
            }
        }
        entitiesSent++;
    }

    if (entitiesSent > 0) {
        auto data = updatePacket.GetSerializedData();
        sendCallback(data.data(), data.size());
        
        state.dirtyComponents.clear();
        state.isDirty = false;
        
        LOG_DEBUG("Replication", "Sent %zu entities to client %llu (%zu bytes)",
                  entitiesSent, clientId, data.size());
    } else {
        LOG_DEBUG("Replication", "No entities to send for client %llu", clientId);
        state.isDirty = false;
    }
}

void ReplicationManager::ReceiveRPC(ClientId clientId, uint32_t rpcId, 
                                     const uint8_t* payload, size_t size) {
    std::lock_guard<std::mutex> lock(_rpcMutex);
    
    auto it = _pimpl->rpcRegistry.find(rpcId);
    if (it == _pimpl->rpcRegistry.end()) {
        LOG_WARN("RPC", "Unknown RPC ID %u from client %llu", rpcId, clientId);
        return;
    }

    const RPCDefinition& def = it->second;
    
    if (def.requiresAuthority && _pimpl->authorityManager) {
        if (!_pimpl->authorityManager->HasAuthority(rpcId)) {
            LOG_WARN("RPC", "Authority denied for client %llu on RPC %u", clientId, rpcId);
            return;
        }
    }

    try {
        def.serverHandler(clientId, payload, size);
    } catch (const std::exception& e) {
        LOG_ERROR("RPC", "Handler exception for RPC %u: %s", rpcId, e.what());
    }
}

void ReplicationManager::RegisterRPCInternal(const RPCDefinition& def) {
    std::lock_guard<std::mutex> lock(_rpcMutex);
    _pimpl->rpcRegistry[def.id] = def;
    LOG_INFO("RPC", "Registered: %s (ID=%u, auth=%s)", 
             def.name.c_str(), def.id, def.requiresAuthority ? "yes" : "no");
}

bool ReplicationManager::CheckAuthority(ClientId clientId, EntityId entityId) {
    if (!_pimpl->authorityManager) return true;
    return _pimpl->authorityManager->HasAuthority(static_cast<uint32_t>(entityId));
}

void ReplicationManager::ProcessEntityDelta(ClientReplicationState& clientState, 
                                             EntityId entityId) {
    auto it = clientState.entityComponentMasks.find(entityId);
    if (it == clientState.entityComponentMasks.end()) {
        it = clientState.entityComponentMasks.emplace(entityId, 0).first;
    }
}

} // namespace SagaEngine::Networking::Replication