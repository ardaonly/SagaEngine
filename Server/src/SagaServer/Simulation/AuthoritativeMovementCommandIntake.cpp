/// @file AuthoritativeMovementCommandIntake.cpp
/// @brief AuthoritativeMovementCommandIntake implementation.

#include "SagaServer/Simulation/AuthoritativeMovementCommandIntake.h"

#include "SagaEngine/Input/Commands/InputCommandSerializer.h"

#include <span>

namespace SagaEngine::Server::Simulation
{

AuthoritativeMovementCommandIntake::AuthoritativeMovementCommandIntake(
    AuthoritativeMovementCommandIntakeConfig config)
    : m_InputAdapter(config.inputAdapter)
{
}

bool AuthoritativeMovementCommandIntake::RegisterActor(ClientId clientId,
                                                       EntityId entityId,
                                                       Vector3  initialPosition)
{
    return m_InputAdapter.RegisterActor(clientId, entityId, initialPosition);
}

void AuthoritativeMovementCommandIntake::UnregisterActor(ClientId clientId)
{
    m_InputAdapter.UnregisterActor(clientId);
}

AuthoritativeMovementCommandIntakeResult
AuthoritativeMovementCommandIntake::HandlePacket(
    const AuthoritativeMovementCommandPacketView& packet)
{
    AuthoritativeMovementCommandIntakeResult result;

    if (packet.packetType != ::SagaEngine::Networking::PacketType::InputCommand)
    {
        result.decision = AuthoritativeMovementCommandIntakeDecision::IgnoredNonInput;
        return result;
    }

    if (!packet.payload ||
        packet.payloadSize != ::SagaEngine::Input::InputCommandSerializer::kWireSize)
    {
        result.decision = AuthoritativeMovementCommandIntakeDecision::MalformedInput;
        return result;
    }

    ::SagaEngine::Input::InputCommand command{};
    const auto deserializeResult = ::SagaEngine::Input::InputCommandSerializer::Deserialize(
        std::span<const std::uint8_t>(packet.payload, packet.payloadSize),
        command);

    if (deserializeResult !=
        ::SagaEngine::Input::InputCommandSerializer::DeserializeResult::Ok)
    {
        result.decision = AuthoritativeMovementCommandIntakeDecision::MalformedInput;
        return result;
    }

    result.decision = AuthoritativeMovementCommandIntakeDecision::Forwarded;
    result.decodedSequence = command.sequence;
    result.movementResult = m_InputAdapter.EnqueueNetworkInput(
        packet.clientId,
        command,
        packet.serverTick,
        packet.recvTimeUnixMs);
    return result;
}

AuthoritativeMovementTickReport AuthoritativeMovementCommandIntake::Tick(
    std::uint64_t serverTick,
    float         dtSeconds)
{
    return m_InputAdapter.Tick(serverTick, dtSeconds);
}

std::optional<Vector3> AuthoritativeMovementCommandIntake::GetActorPosition(
    EntityId entityId) const
{
    return m_InputAdapter.GetActorPosition(entityId);
}

std::vector<EntityId> AuthoritativeMovementCommandIntake::ConsumeDirtyEntities()
{
    return m_InputAdapter.ConsumeDirtyEntities();
}

} // namespace SagaEngine::Server::Simulation
