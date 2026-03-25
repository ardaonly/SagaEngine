#pragma once
#include "SagaEngine/Networking/Core/NetworkTypes.h"
#include "SagaEngine/ECS/Entity.h"
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <mutex>
#include <memory>
#include <cmath>

namespace SagaEngine::Networking::Interest {

using EntityId = ECS::EntityId;
using ClientId = ::SagaEngine::Networking::ClientId;
using AreaId = uint32_t;

struct Vector3 {
    float x{0.0f}, y{0.0f}, z{0.0f};
    
    Vector3() = default;
    Vector3(float x, float y, float z) : x(x), y(y), z(z) {}
    
    float DistanceTo(const Vector3& other) const {
        float dx = x - other.x;
        float dy = y - other.y;
        float dz = z - other.z;
        return std::sqrt(dx*dx + dy*dy + dz*dz);
    }
    
    bool operator==(const Vector3& o) const {
        return x == o.x && y == o.y && z == o.z;
    }
};

struct InterestConfig {
    float viewRadius{100.0f};
    float cullRadius{150.0f};
    uint32_t maxEntitiesPerUpdate{64};
    float priorityDistanceWeight{1.0f};
    float priorityImportanceWeight{2.0f};
    float updateFrequency{20.0f};
};

class InterestArea {
public:
    InterestArea(AreaId id, const Vector3& center, float radius);
    ~InterestArea() = default;
    
    AreaId GetId() const { return _id; }
    const Vector3& GetCenter() const { return _center; }
    float GetRadius() const { return _radius; }
    
    bool Contains(const Vector3& position) const;
    float DistanceTo(const Vector3& position) const;
    
    void AddEntity(EntityId entity);
    void RemoveEntity(EntityId entity);
    const std::unordered_set<EntityId>& GetEntities() const { return _entities; }
    
    void Subscribe(ClientId client);
    void Unsubscribe(ClientId client);
    const std::unordered_set<ClientId>& GetSubscribers() const { return _subscribers; }
    
    bool IsSubscribed(ClientId client) const;
    
private:
    AreaId _id;
    Vector3 _center;
    float _radius;
    std::unordered_set<EntityId> _entities;
    std::unordered_set<ClientId> _subscribers;
    mutable std::mutex _mutex;
};

class InterestManager {
public:
    explicit InterestManager(InterestConfig config = {});
    ~InterestManager() = default;
    
    void SetConfig(const InterestConfig& config) { _config = config; }
    const InterestConfig& GetConfig() const { return _config; }
    
    AreaId CreateArea(const Vector3& center, float radius);
    void DestroyArea(AreaId areaId);
    
    void UpdateEntityPosition(EntityId entity, const Vector3& position);
    void RemoveEntity(EntityId entity);
    
    std::vector<EntityId> GetVisibleEntities(ClientId client) const;
    std::vector<EntityId> GetEntitiesInArea(AreaId areaId) const;
    
    void SubscribeClientToArea(ClientId client, AreaId areaId);
    void UnsubscribeClientFromArea(ClientId client, AreaId areaId);
    
    std::vector<AreaId> GetClientSubscribedAreas(ClientId client) const;
    AreaId GetEntityArea(EntityId entity) const;
    
    void Update(float dt);
    
private:
    InterestConfig _config;
    mutable std::mutex _mutex;
    
    std::unordered_map<AreaId, std::unique_ptr<InterestArea>> _areas;
    std::unordered_map<EntityId, Vector3> _entityPositions;
    std::unordered_map<EntityId, AreaId> _entityAreas;
    std::unordered_map<ClientId, std::unordered_set<AreaId>> _clientSubscriptions;
    
    AreaId _nextAreaId{1};
    float _updateAccumulator{0.0f};
    
    AreaId FindBestArea(const Vector3& position) const;
    float CalculatePriority(ClientId client, EntityId entity) const;
    void UpdateEntityArea(EntityId entity);
};

} // namespace SagaEngine::Networking::Interest