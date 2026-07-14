/// @file AuthoritativeMovementInputAdapter.h
/// @brief Converts production-shaped Engine input into the server movement core.

#pragma once

#include "SagaServer/Simulation/AuthoritativeMovementCore.h"
#include "SagaEngine/Input/Commands/InputCommand.h"

#include <cstdint>
#include <mutex>
#include <optional>
#include <unordered_map>
#include <vector>

namespace SagaEngine::Server::Simulation
{

/// Configuration for the Engine-input to server-core adapter.
struct AuthoritativeMovementInputAdapterConfig
{
    AuthoritativeMovementCoreConfig movementCore;
    std::uint8_t expectedInputVersion{::SagaEngine::Input::InputCommand::kCurrentVersion};
};

/// Narrow adapter between Engine's network InputCommand and AuthoritativeMovementCore.
class AuthoritativeMovementInputAdapter final
{
public:
    explicit AuthoritativeMovementInputAdapter(
        AuthoritativeMovementInputAdapterConfig config = {});
    ~AuthoritativeMovementInputAdapter() = default;

    AuthoritativeMovementInputAdapter(const AuthoritativeMovementInputAdapter&) = delete;
    AuthoritativeMovementInputAdapter& operator=(
        const AuthoritativeMovementInputAdapter&) = delete;

    [[nodiscard]] bool RegisterActor(ClientId clientId,
                                     EntityId entityId,
                                     Vector3  initialPosition);
    void UnregisterActor(ClientId clientId);

    [[nodiscard]] AuthoritativeMovementResult EnqueueNetworkInput(
        ClientId                                clientId,
        const ::SagaEngine::Input::InputCommand& command,
        std::uint64_t                           serverRecvTick,
        std::uint64_t                           recvTimeUnixMs);

    [[nodiscard]] AuthoritativeMovementTickReport Tick(std::uint64_t serverTick,
                                                        float         dtSeconds);

    [[nodiscard]] std::optional<Vector3> GetActorPosition(EntityId entityId) const;
    [[nodiscard]] std::vector<EntityId>  ConsumeDirtyEntities();

private:
    [[nodiscard]] InputCommand ConvertInput(
        const ::SagaEngine::Input::InputCommand& command,
        std::uint64_t                            serverRecvTick,
        std::uint64_t                            recvTimeUnixMs) const noexcept;

    [[nodiscard]] AuthoritativeMovementResult MakeRejectedInputResult(
        ClientId                                clientId,
        EntityId                                entityId,
        const ::SagaEngine::Input::InputCommand& command,
        std::uint64_t                           serverRecvTick,
        AuthoritativeMovementDecision           decision) const;

    AuthoritativeMovementInputAdapterConfig m_Config;
    AuthoritativeMovementCore               m_Core;

    mutable std::mutex                      m_Mutex;
    std::unordered_map<ClientId, EntityId>  m_ClientActors;
};

} // namespace SagaEngine::Server::Simulation
