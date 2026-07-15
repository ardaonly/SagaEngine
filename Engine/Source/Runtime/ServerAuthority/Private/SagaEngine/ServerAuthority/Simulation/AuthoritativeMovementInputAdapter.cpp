/// @file AuthoritativeMovementInputAdapter.cpp
/// @brief AuthoritativeMovementInputAdapter implementation.

#include "SagaEngine/ServerAuthority/Simulation/AuthoritativeMovementInputAdapter.h"

namespace SagaEngine::ServerAuthority::Simulation
{

AuthoritativeMovementInputAdapter::AuthoritativeMovementInputAdapter(
    AuthoritativeMovementInputAdapterConfig config)
    : m_Config(config)
    , m_Core(m_Config.movementCore)
{
}

bool AuthoritativeMovementInputAdapter::RegisterActor(ClientId clientId,
                                                      EntityId entityId,
                                                      Vector3  initialPosition)
{
    std::lock_guard<std::mutex> lock(m_Mutex);

    const bool registered = m_Core.RegisterActor(clientId, entityId, initialPosition);
    if (!registered)
        return false;

    m_ClientActors[clientId] = entityId;
    return true;
}

void AuthoritativeMovementInputAdapter::UnregisterActor(ClientId clientId)
{
    std::lock_guard<std::mutex> lock(m_Mutex);
    m_ClientActors.erase(clientId);
    m_Core.UnregisterActor(clientId);
}

AuthoritativeMovementResult AuthoritativeMovementInputAdapter::EnqueueNetworkInput(
    ClientId                                  clientId,
    const ::SagaEngine::Input::InputCommand& command,
    std::uint64_t                            serverRecvTick,
    std::uint64_t                            recvTimeUnixMs)
{
    std::lock_guard<std::mutex> lock(m_Mutex);

    EntityId entityId{0};
    const auto it = m_ClientActors.find(clientId);
    if (it == m_ClientActors.end())
    {
        return MakeRejectedInputResult(
            clientId,
            entityId,
            command,
            serverRecvTick,
            AuthoritativeMovementDecision::RejectUnknownClient);
    }

    entityId = it->second;

    if (command.version != m_Config.expectedInputVersion)
    {
        return MakeRejectedInputResult(
            clientId,
            entityId,
            command,
            serverRecvTick,
            AuthoritativeMovementDecision::RejectInput);
    }

    return m_Core.EnqueueInput(
        clientId,
        entityId,
        ConvertInput(command, serverRecvTick, recvTimeUnixMs));
}

AuthoritativeMovementTickReport AuthoritativeMovementInputAdapter::Tick(
    std::uint64_t serverTick,
    float         dtSeconds)
{
    std::lock_guard<std::mutex> lock(m_Mutex);
    return m_Core.Tick(serverTick, dtSeconds);
}

std::optional<Vector3> AuthoritativeMovementInputAdapter::GetActorPosition(
    EntityId entityId) const
{
    std::lock_guard<std::mutex> lock(m_Mutex);
    return m_Core.GetActorPosition(entityId);
}

std::vector<EntityId> AuthoritativeMovementInputAdapter::ConsumeDirtyEntities()
{
    std::lock_guard<std::mutex> lock(m_Mutex);
    return m_Core.ConsumeDirtyEntities();
}

InputCommand AuthoritativeMovementInputAdapter::ConvertInput(
    const ::SagaEngine::Input::InputCommand& command,
    std::uint64_t                            serverRecvTick,
    std::uint64_t                            recvTimeUnixMs) const noexcept
{
    InputCommand out;
    out.sequence = command.sequence;
    out.clientTick = command.simulationTick;
    out.serverRecvTick = serverRecvTick;
    out.recvTimeUnixMs = recvTimeUnixMs;
    out.moveX = ::SagaEngine::Input::FloatFromFixed(command.moveX);
    out.moveY = 0.0f;
    out.moveZ = ::SagaEngine::Input::FloatFromFixed(command.moveY);
    out.lookYaw = ::SagaEngine::Input::FloatFromFixed(command.lookX);
    out.lookPitch = ::SagaEngine::Input::FloatFromFixed(command.lookY);
    out.buttonMask = command.buttons;
    return out;
}

AuthoritativeMovementResult
AuthoritativeMovementInputAdapter::MakeRejectedInputResult(
    ClientId                                  clientId,
    EntityId                                  entityId,
    const ::SagaEngine::Input::InputCommand& command,
    std::uint64_t                            serverRecvTick,
    AuthoritativeMovementDecision            decision) const
{
    AuthoritativeMovementResult result;
    result.clientId = clientId;
    result.entityId = entityId;
    result.inputSequence = command.sequence;
    result.serverTick = serverRecvTick;
    result.decision = decision;
    return result;
}

} // namespace SagaEngine::ServerAuthority::Simulation
