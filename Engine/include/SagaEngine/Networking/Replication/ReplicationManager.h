#pragma once
#include "SagaEngine/Networking/Core/Packet.h"
#include "SagaEngine/Networking/Core/NetworkTypes.h"
#include "SagaEngine/ECS/Entity.h"
#include "SagaEngine/ECS/Component.h"
#include "SagaEngine/ECS/Serialization.h"
#include "SagaEngine/Networking/Interest/InterestArea.h"
#include <memory>
#include <vector>
#include <unordered_map>
#include <functional>
#include <mutex>
#include <cstdint>

namespace SagaEngine::Simulation {
    class WorldState;
    class AuthorityManager;
}

namespace SagaEngine::Networking::Replication {

using EntityId = ECS::EntityId;
using ComponentTypeId = ECS::ComponentTypeId;
using ClientId = ::SagaEngine::Networking::ClientId;

struct ReplicationPriority {
    float distanceScore{0.0f};
    float importanceScore{0.0f};
    uint32_t lastReplicatedTick{0};
    
    float CalculateScore() const {
        return (importanceScore * 2.0f) + (1.0f / (distanceScore + 0.1f));
    }
};

struct ClientReplicationState {
    ClientId clientId;
    std::unordered_map<EntityId, uint64_t> entityComponentMasks;
    std::unordered_map<EntityId, ReplicationPriority> priorities;
    std::unordered_map<EntityId, std::vector<ComponentTypeId>> dirtyComponents;
    uint64_t lastAckedSequence{0};
    bool isDirty{false};
};

struct RPCDefinition {
    using Func = std::function<void(ClientId, const uint8_t*, size_t)>;
    std::string name;
    uint32_t id;
    Func serverHandler;
    bool requiresAuthority{true};
};

class ReplicationManager {
public:
    ReplicationManager();
    ~ReplicationManager();

    ReplicationManager(const ReplicationManager&) = delete;
    ReplicationManager& operator=(const ReplicationManager&) = delete;

    void Initialize(uint32_t maxClients = 1000);
    void Shutdown();

    void AddClient(ClientId clientId);
    void RemoveClient(ClientId clientId);
    bool HasClient(ClientId clientId) const;

    void GatherDirtyEntities(float dt, bool force = false);
    void SendUpdates(ClientId clientId, std::function<void(const uint8_t*, size_t)> sendCallback);
    void ReceiveRPC(ClientId clientId, uint32_t rpcId, const uint8_t* payload, size_t size);

    void RegisterEntityForReplication(EntityId entityId);
    void UnregisterEntity(EntityId entityId);
    void MarkComponentDirty(EntityId entityId, ComponentTypeId componentId);

    template<typename Func>
    void RegisterRPC(const char* name, Func handler, bool requiresAuth = true);

    void SetReplicationFrequency(float hz);
    float GetReplicationFrequency() const { return _replicationHz; }

    void SetWorldState(Simulation::WorldState* world);
    void SetAuthorityManager(Simulation::AuthorityManager* auth);
    void SetInterestManager(Interest::InterestManager* interest);

private:
    struct Impl;
    std::unique_ptr<Impl> _pimpl;
    
    float _replicationHz{20.0f};
    float _accumulator{0.0f};
    mutable std::mutex _stateMutex;
    mutable std::mutex _rpcMutex;
    
    void ProcessEntityDelta(ClientReplicationState& clientState, EntityId entityId);
    bool CheckAuthority(ClientId clientId, EntityId entityId);
    void RegisterRPCInternal(const RPCDefinition& def);
};

template<typename Func>
void ReplicationManager::RegisterRPC(const char* name, Func handler, bool requiresAuth) {
    static uint32_t s_NextId = 1;
    RPCDefinition def;
    def.name = name;
    def.id = s_NextId++;
    def.requiresAuthority = requiresAuth;
    def.serverHandler = [handler](ClientId cid, const uint8_t* p, size_t s) {
        handler(cid, p, s);
    };
    RegisterRPCInternal(def);
}

} // namespace SagaEngine::Networking::Replication