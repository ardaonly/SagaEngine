/// @file ActorOwnershipRegistry.h
/// @brief Server-side client-to-controlled-actor ownership contract.

#pragma once

#include "SagaServer/Simulation/InputCommandQueue.h"
#include "SagaServer/Simulation/MovementValidator.h"

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <optional>
#include <unordered_map>

namespace SagaEngine::Server::Simulation
{

/// Actor controlled by one connected client.
struct ControlledActor
{
    ClientId clientId{0};
    EntityId entityId{0};
    Vector3  initialPosition{};
};

/// Result for ownership lifecycle operations.
enum class ActorOwnershipResult : std::uint8_t
{
    Registered = 0,
    AlreadyRegistered,
    Unregistered,
    NotFound,
    InvalidClient,
    InvalidEntity,
    ClientAlreadyOwnsDifferentEntity,
    EntityAlreadyOwnedByDifferentClient
};

/// Bidirectional registry for the production ClientId -> EntityId control mapping.
class ActorOwnershipRegistry final
{
public:
    ActorOwnershipRegistry() = default;
    ~ActorOwnershipRegistry() = default;

    ActorOwnershipRegistry(const ActorOwnershipRegistry&)            = delete;
    ActorOwnershipRegistry& operator=(const ActorOwnershipRegistry&) = delete;

    [[nodiscard]] ActorOwnershipResult RegisterOwnership(ClientId clientId,
                                                          EntityId entityId,
                                                          Vector3  initialPosition);
    [[nodiscard]] ActorOwnershipResult UnregisterClient(ClientId clientId);
    [[nodiscard]] ActorOwnershipResult UnregisterEntity(EntityId entityId);

    [[nodiscard]] std::optional<ControlledActor> FindByClient(ClientId clientId) const;
    [[nodiscard]] std::optional<ControlledActor> FindByEntity(EntityId entityId) const;
    [[nodiscard]] bool Owns(ClientId clientId, EntityId entityId) const;
    [[nodiscard]] std::size_t Count() const;

    void Clear();

private:
    mutable std::mutex m_Mutex;
    std::unordered_map<ClientId, ControlledActor> m_ByClient;
    std::unordered_map<EntityId, ClientId>        m_ByEntity;
};

} // namespace SagaEngine::Server::Simulation
