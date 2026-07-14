/// @file ActorOwnershipRegistry.cpp
/// @brief ActorOwnershipRegistry implementation.

#include "SagaServer/Simulation/ActorOwnershipRegistry.h"

namespace SagaEngine::Server::Simulation
{

ActorOwnershipResult ActorOwnershipRegistry::RegisterOwnership(
    ClientId clientId,
    EntityId entityId,
    Vector3  initialPosition)
{
    if (clientId == 0)
    {
        return ActorOwnershipResult::InvalidClient;
    }

    if (entityId == 0)
    {
        return ActorOwnershipResult::InvalidEntity;
    }

    std::lock_guard<std::mutex> lock(m_Mutex);

    const auto clientIt = m_ByClient.find(clientId);
    if (clientIt != m_ByClient.end())
    {
        if (clientIt->second.entityId == entityId)
        {
            return ActorOwnershipResult::AlreadyRegistered;
        }

        return ActorOwnershipResult::ClientAlreadyOwnsDifferentEntity;
    }

    const auto entityIt = m_ByEntity.find(entityId);
    if (entityIt != m_ByEntity.end())
    {
        return ActorOwnershipResult::EntityAlreadyOwnedByDifferentClient;
    }

    m_ByClient.emplace(clientId, ControlledActor{clientId, entityId, initialPosition});
    m_ByEntity.emplace(entityId, clientId);
    return ActorOwnershipResult::Registered;
}

ActorOwnershipResult ActorOwnershipRegistry::UnregisterClient(ClientId clientId)
{
    if (clientId == 0)
    {
        return ActorOwnershipResult::InvalidClient;
    }

    std::lock_guard<std::mutex> lock(m_Mutex);

    const auto clientIt = m_ByClient.find(clientId);
    if (clientIt == m_ByClient.end())
    {
        return ActorOwnershipResult::NotFound;
    }

    m_ByEntity.erase(clientIt->second.entityId);
    m_ByClient.erase(clientIt);
    return ActorOwnershipResult::Unregistered;
}

ActorOwnershipResult ActorOwnershipRegistry::UnregisterEntity(EntityId entityId)
{
    if (entityId == 0)
    {
        return ActorOwnershipResult::InvalidEntity;
    }

    std::lock_guard<std::mutex> lock(m_Mutex);

    const auto entityIt = m_ByEntity.find(entityId);
    if (entityIt == m_ByEntity.end())
    {
        return ActorOwnershipResult::NotFound;
    }

    m_ByClient.erase(entityIt->second);
    m_ByEntity.erase(entityIt);
    return ActorOwnershipResult::Unregistered;
}

std::optional<ControlledActor> ActorOwnershipRegistry::FindByClient(
    ClientId clientId) const
{
    std::lock_guard<std::mutex> lock(m_Mutex);

    const auto clientIt = m_ByClient.find(clientId);
    if (clientIt == m_ByClient.end())
    {
        return std::nullopt;
    }

    return clientIt->second;
}

std::optional<ControlledActor> ActorOwnershipRegistry::FindByEntity(
    EntityId entityId) const
{
    std::lock_guard<std::mutex> lock(m_Mutex);

    const auto entityIt = m_ByEntity.find(entityId);
    if (entityIt == m_ByEntity.end())
    {
        return std::nullopt;
    }

    const auto clientIt = m_ByClient.find(entityIt->second);
    if (clientIt == m_ByClient.end())
    {
        return std::nullopt;
    }

    return clientIt->second;
}

bool ActorOwnershipRegistry::Owns(ClientId clientId, EntityId entityId) const
{
    std::lock_guard<std::mutex> lock(m_Mutex);

    const auto clientIt = m_ByClient.find(clientId);
    return clientIt != m_ByClient.end() && clientIt->second.entityId == entityId;
}

std::size_t ActorOwnershipRegistry::Count() const
{
    std::lock_guard<std::mutex> lock(m_Mutex);
    return m_ByClient.size();
}

void ActorOwnershipRegistry::Clear()
{
    std::lock_guard<std::mutex> lock(m_Mutex);
    m_ByClient.clear();
    m_ByEntity.clear();
}

} // namespace SagaEngine::Server::Simulation
