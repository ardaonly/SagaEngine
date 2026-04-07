/// @file ReplicationManager.h
/// @brief Replication manager for server-side entity/component delta delivery.

#pragma once

#include "SagaServer/Networking/Core/Packet.h"
#include "SagaServer/Networking/Core/NetworkTypes.h"
#include "SagaServer/Networking/Interest/InterestArea.h"
#include "SagaEngine/ECS/Entity.h"
#include "SagaEngine/ECS/Component.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace SagaEngine::Simulation
{
    class AuthorityManager;
    class WorldState;
}

namespace SagaEngine::Networking::Replication
{

using EntityId        = ECS::EntityId;
using ComponentTypeId  = ECS::ComponentTypeId;
using ClientId        = ::SagaEngine::Networking::ClientId;

/// Snapshot interface used by replication backends that expose read-only world data.
class IWorldSnapshot
{
public:
    virtual ~IWorldSnapshot() = default;

    virtual std::vector<EntityId> GetAllEntities() const = 0;
    virtual bool                  EntityHasComponent(EntityId, ComponentTypeId) const = 0;

    virtual std::size_t GetSerializedComponent(
        EntityId        entityId,
        ComponentTypeId typeId,
        uint8_t*        buf,
        std::size_t     bufSize) const = 0;
};

/// Priority values tracked per entity/client pair.
struct ReplicationPriority
{
    float    distanceScore{0.0f};
    float    importanceScore{0.0f};
    uint32_t lastReplicatedTick{0};

    [[nodiscard]] float CalculateScore() const
    {
        return (importanceScore * 2.0f) + (1.0f / (distanceScore + 0.1f));
    }
};

/// Per-client replication bookkeeping.
struct ClientReplicationState
{
    ClientId clientId{0};

    std::unordered_map<EntityId, std::unordered_set<ComponentTypeId>> sentComponents;
    std::unordered_map<EntityId, std::vector<ComponentTypeId>>        pendingComponents;
    std::unordered_map<EntityId, ReplicationPriority>                 priorities;

    uint64_t lastAckedSequence{0};
    bool     isDirty{false};
};

/// RPC definition stored in the server registry.
struct RPCDefinition
{
    using Func = std::function<void(
        ClientId clientId,
        EntityId targetEntity,
        const uint8_t* payload,
        std::size_t size)>;

    std::string name;
    uint32_t    id{0};
    Func        serverHandler;
    bool        requiresEntityAuthority{false};
};

/// Server-side replication coordinator.
class ReplicationManager
{
public:
    ReplicationManager();
    ~ReplicationManager();

    ReplicationManager(const ReplicationManager&)            = delete;
    ReplicationManager& operator=(const ReplicationManager&) = delete;

    void Initialize(uint32_t maxClients = 1000);
    void Shutdown();

    void AddClient(ClientId clientId);
    void RemoveClient(ClientId clientId);
    [[nodiscard]] bool HasClient(ClientId clientId) const;

    void SetWorldState(Simulation::WorldState* world);

    void GatherDirtyEntities(float dt, bool force = false);
    void SendUpdates(ClientId clientId,
                     std::function<void(const uint8_t*, std::size_t)> sendCallback);

    void ReceiveRPC(ClientId       clientId,
                    uint32_t       rpcId,
                    EntityId       targetEntity,
                    const uint8_t* payload,
                    std::size_t    size);

    void RegisterEntityForReplication(EntityId entityId);
    void UnregisterEntity(EntityId entityId);
    void MarkComponentDirty(EntityId entityId, ComponentTypeId componentId);

    template<typename Func>
    [[nodiscard]] uint32_t RegisterRPC(const char* name, Func handler, bool requiresEntityAuth = false);

    void  SetReplicationFrequency(float hz);
    [[nodiscard]] float GetReplicationFrequency() const { return _replicationHz; }

    void SetWorldSnapshot(IWorldSnapshot* snapshot);
    void SetAuthorityManager(Simulation::AuthorityManager* auth);
    void SetInterestManager(Interest::InterestManager* interest);

private:
    struct Impl;
    std::unique_ptr<Impl> _pimpl;

    float _replicationHz{20.0f};
    float _accumulator{0.0f};

    mutable std::mutex _stateMutex;
    mutable std::mutex _rpcMutex;

    struct PendingUpdate
    {
        EntityId                     entityId;
        std::vector<ComponentTypeId> components;
    };

    [[nodiscard]] std::vector<PendingUpdate> CollectPendingUpdates(ClientId clientId);
    void ProcessEntityDelta(ClientReplicationState& clientState, EntityId entityId);
    bool CheckEntityAuthority(ClientId clientId, EntityId entityId) const;
    void RegisterRPCInternal(const RPCDefinition& def);
};

template<typename Func>
uint32_t ReplicationManager::RegisterRPC(const char* name, Func handler, bool requiresEntityAuth)
{
    static std::atomic<uint32_t> s_NextId{1};

    RPCDefinition def;
    def.name                    = name;
    def.id                      = s_NextId.fetch_add(1, std::memory_order_relaxed);
    def.requiresEntityAuthority = requiresEntityAuth;
    def.serverHandler           = [handler](ClientId cid, EntityId eid, const uint8_t* p, std::size_t s)
    {
        handler(cid, eid, p, s);
    };

    RegisterRPCInternal(def);
    return def.id;
}

} // namespace SagaEngine::Networking::Replication