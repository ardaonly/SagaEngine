#include "SagaServer/Networking/Interest/InterestArea.h"
#include "SagaEngine/Core/Log/Log.h"
#include <algorithm>
#include <limits>

namespace SagaEngine::Networking::Interest {

InterestArea::InterestArea(AreaId id, const Vector3& center, float radius)
    : _id(id)
    , _center(center)
    , _radius(radius)
{
    LOG_DEBUG("InterestArea", "Created area %u at (%.2f, %.2f, %.2f) radius=%.2f",
              id, center.x, center.y, center.z, radius);
}

bool InterestArea::Contains(const Vector3& position) const {
    return _center.DistanceTo(position) <= _radius;
}

float InterestArea::DistanceTo(const Vector3& position) const {
    return _center.DistanceTo(position);
}

void InterestArea::AddEntity(EntityId entity) {
    std::lock_guard<std::mutex> lock(_mutex);
    _entities.insert(entity);
}

void InterestArea::RemoveEntity(EntityId entity) {
    std::lock_guard<std::mutex> lock(_mutex);
    _entities.erase(entity);
}

void InterestArea::Subscribe(ClientId client) {
    std::lock_guard<std::mutex> lock(_mutex);
    _subscribers.insert(client);
    LOG_DEBUG("InterestArea", "Client %llu subscribed to area %u", client, _id);
}

void InterestArea::Unsubscribe(ClientId client) {
    std::lock_guard<std::mutex> lock(_mutex);
    _subscribers.erase(client);
    LOG_DEBUG("InterestArea", "Client %llu unsubscribed from area %u", client, _id);
}

bool InterestArea::IsSubscribed(ClientId client) const {
    std::lock_guard<std::mutex> lock(_mutex);
    return _subscribers.find(client) != _subscribers.end();
}

InterestManager::InterestManager(InterestConfig config)
    : _config(config)
{
    LOG_INFO("InterestManager", "Initialized (viewRadius=%.2f, updateFrequency=%.2fHz)",
             _config.viewRadius, _config.updateFrequency);
}

AreaId InterestManager::CreateArea(const Vector3& center, float radius) {
    std::lock_guard<std::mutex> lock(_mutex);
    AreaId id = _nextAreaId++;
    _areas[id] = std::make_unique<InterestArea>(id, center, radius);
    LOG_INFO("InterestManager", "Created area %u", id);
    return id;
}

void InterestManager::DestroyArea(AreaId areaId) {
    std::lock_guard<std::mutex> lock(_mutex);
    auto it = _areas.find(areaId);
    if (it != _areas.end()) {
        for (auto& [client, subscriptions] : _clientSubscriptions) {
            subscriptions.erase(areaId);
        }
        for (auto entityIt = _entityAreas.begin(); entityIt != _entityAreas.end();) {
            if (entityIt->second == areaId) {
                entityIt = _entityAreas.erase(entityIt);
            } else {
                ++entityIt;
            }
        }
        _areas.erase(it);
        LOG_INFO("InterestManager", "Destroyed area %u", areaId);
    }
}

void InterestManager::UpdateEntityPosition(EntityId entity, const Vector3& position) {
    std::lock_guard<std::mutex> lock(_mutex);
    _entityPositions[entity] = position;
    UpdateEntityArea(entity);
}

void InterestManager::RemoveEntity(EntityId entity) {
    std::lock_guard<std::mutex> lock(_mutex);
    
    auto areaIt = _entityAreas.find(entity);
    if (areaIt != _entityAreas.end()) {
        auto area = _areas.find(areaIt->second);
        if (area != _areas.end()) {
            area->second->RemoveEntity(entity);
        }
        _entityAreas.erase(areaIt);
    }
    
    _entityPositions.erase(entity);
}

void InterestManager::UpdateEntityArea(EntityId entity) {
    auto posIt = _entityPositions.find(entity);
    if (posIt == _entityPositions.end()) return;

    AreaId bestArea = FindBestArea(posIt->second);
    AreaId currentArea = _entityAreas[entity];

    if (bestArea != currentArea) {
        if (currentArea != 0) {
            auto oldArea = _areas.find(currentArea);
            if (oldArea != _areas.end()) {
                oldArea->second->RemoveEntity(entity);
            }
        }
        if (bestArea != 0) {
            auto newArea = _areas.find(bestArea);
            if (newArea != _areas.end()) {
                newArea->second->AddEntity(entity);
                _entityAreas[entity] = bestArea;
                LOG_DEBUG("InterestManager", "Entity %u moved from area %u to %u",
                          entity, currentArea, bestArea);
            }
        } else {
            _entityAreas.erase(entity);
            LOG_DEBUG("InterestManager", "Entity %u left area %u", entity, currentArea);
        }
    }
}

AreaId InterestManager::FindBestArea(const Vector3& position) const {
    AreaId bestId = 0;
    float bestDistance = std::numeric_limits<float>::max();
    
    for (const auto& [id, area] : _areas) {
        if (area->Contains(position)) {
            float dist = area->DistanceTo(position);
            if (dist < bestDistance) {
                bestDistance = dist;
                bestId = id;
            }
        }
    }
    
    return bestId;
}

void InterestManager::SubscribeClientToArea(ClientId client, AreaId areaId) {
    std::lock_guard<std::mutex> lock(_mutex);
    
    auto areaIt = _areas.find(areaId);
    if (areaIt == _areas.end()) {
        LOG_WARN("InterestManager", "Cannot subscribe client %llu to non-existent area %u",
                 client, areaId);
        return;
    }
    
    _clientSubscriptions[client].insert(areaId);
    areaIt->second->Subscribe(client);
}

void InterestManager::UnsubscribeClientFromArea(ClientId client, AreaId areaId) {
    std::lock_guard<std::mutex> lock(_mutex);
    
    auto areaIt = _areas.find(areaId);
    if (areaIt != _areas.end()) {
        areaIt->second->Unsubscribe(client);
    }
    
    auto subIt = _clientSubscriptions.find(client);
    if (subIt != _clientSubscriptions.end()) {
        subIt->second.erase(areaId);
        if (subIt->second.empty()) {
            _clientSubscriptions.erase(subIt);
        }
    }
}

std::vector<AreaId> InterestManager::GetClientSubscribedAreas(ClientId client) const {
    std::lock_guard<std::mutex> lock(_mutex);
    
    std::vector<AreaId> result;
    auto it = _clientSubscriptions.find(client);
    if (it != _clientSubscriptions.end()) {
        result.assign(it->second.begin(), it->second.end());
    }
    return result;
}

AreaId InterestManager::GetEntityArea(EntityId entity) const {
    std::lock_guard<std::mutex> lock(_mutex);
    
    auto it = _entityAreas.find(entity);
    return (it != _entityAreas.end()) ? it->second : 0;
}

std::vector<EntityId> InterestManager::GetEntitiesInArea(AreaId areaId) const {
    std::lock_guard<std::mutex> lock(_mutex);
    
    std::vector<EntityId> result;
    auto it = _areas.find(areaId);
    if (it != _areas.end()) {
        const auto& entities = it->second->GetEntities();
        result.assign(entities.begin(), entities.end());
    }
    return result;
}

std::vector<EntityId> InterestManager::GetVisibleEntities(ClientId client) const {
    std::lock_guard<std::mutex> lock(_mutex);
    
    std::unordered_set<EntityId> visible;
    
    auto subIt = _clientSubscriptions.find(client);
    if (subIt == _clientSubscriptions.end()) {
        return {};
    }
    
    for (AreaId areaId : subIt->second) {
        auto areaIt = _areas.find(areaId);
        if (areaIt != _areas.end()) {
            for (EntityId entity : areaIt->second->GetEntities()) {
                visible.insert(entity);
            }
        }
    }
    
    auto posIt = _entityPositions.find(static_cast<EntityId>(client));
    if (posIt != _entityPositions.end()) {
        const Vector3& clientPos = posIt->second;
        std::vector<EntityId> culled;

        for (EntityId entity : visible) {
            auto entityPosIt = _entityPositions.find(entity);
            if (entityPosIt != _entityPositions.end()) {
                float dist = clientPos.DistanceTo(entityPosIt->second);
                if (dist <= _config.cullRadius) {
                    culled.push_back(entity);
                }
            }
        }
        
        return culled;
    }
    
    return std::vector<EntityId>(visible.begin(), visible.end());
}

float InterestManager::CalculatePriority(ClientId client, EntityId entity) const {
    auto clientPosIt = _entityPositions.find(static_cast<EntityId>(client));
    auto entityPosIt = _entityPositions.find(entity);
    
    if (clientPosIt == _entityPositions.end() || entityPosIt == _entityPositions.end()) {
        return 0.0f;
    }
    
    float dist = clientPosIt->second.DistanceTo(entityPosIt->second);
    float normalizedDist = 1.0f / (dist + 1.0f);
    
    return normalizedDist * _config.priorityDistanceWeight;
}

void InterestManager::Update(float dt) {
    _updateAccumulator += dt;
    float updateInterval = 1.0f / _config.updateFrequency;
    
    if (_updateAccumulator < updateInterval) {
        return;
    }
    _updateAccumulator = 0.0f;
    
    std::lock_guard<std::mutex> lock(_mutex);
    
    for (auto& [entity, position] : _entityPositions) {
        UpdateEntityArea(entity);
    }
    
    LOG_DEBUG("InterestManager", "Update complete (areas=%zu, entities=%zu, clients=%zu)",
              _areas.size(), _entityPositions.size(), _clientSubscriptions.size());
}

} // namespace SagaEngine::Networking::Interest