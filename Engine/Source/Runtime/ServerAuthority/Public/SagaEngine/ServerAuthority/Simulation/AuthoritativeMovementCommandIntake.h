/// @file AuthoritativeMovementCommandIntake.h
/// @brief Normalized packet/session intake for authoritative movement commands.

#pragma once

#include "SagaEngine/ServerAuthority/Simulation/AuthoritativeMovementInputAdapter.h"
#include "SagaEngine/Networking/NetworkTypes.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace SagaEngine::ServerAuthority::Simulation
{

/// Configuration for the normalized command-intake seam.
struct AuthoritativeMovementCommandIntakeConfig
{
    AuthoritativeMovementInputAdapterConfig inputAdapter;
};

/// Packet data after transport/session code has identified the sender and type.
struct AuthoritativeMovementCommandPacketView
{
    ClientId                         clientId{0};
    ::SagaEngine::Networking::PacketType packetType{
        ::SagaEngine::Networking::PacketType::Invalid};
    const std::uint8_t*              payload{nullptr};
    std::size_t                      payloadSize{0};
    std::uint64_t                    serverTick{0};
    std::uint64_t                    recvTimeUnixMs{0};
};

/// Intake-level decision before or after forwarding to the movement adapter.
enum class AuthoritativeMovementCommandIntakeDecision : std::uint8_t
{
    IgnoredNonInput = 0,
    MalformedInput,
    Forwarded
};

/// Observable result for one normalized packet intake attempt.
struct AuthoritativeMovementCommandIntakeResult
{
    AuthoritativeMovementCommandIntakeDecision decision{
        AuthoritativeMovementCommandIntakeDecision::IgnoredNonInput};
    std::uint64_t                              decodedSequence{0};
    std::optional<AuthoritativeMovementResult> movementResult;
};

/// Decodes movement packets and forwards valid commands to the authoritative core.
class AuthoritativeMovementCommandIntake final
{
public:
    explicit AuthoritativeMovementCommandIntake(
        AuthoritativeMovementCommandIntakeConfig config = {});
    ~AuthoritativeMovementCommandIntake() = default;

    AuthoritativeMovementCommandIntake(const AuthoritativeMovementCommandIntake&) = delete;
    AuthoritativeMovementCommandIntake& operator=(
        const AuthoritativeMovementCommandIntake&) = delete;

    [[nodiscard]] bool RegisterActor(ClientId clientId,
                                     EntityId entityId,
                                     Vector3  initialPosition);
    void UnregisterActor(ClientId clientId);

    [[nodiscard]] AuthoritativeMovementCommandIntakeResult HandlePacket(
        const AuthoritativeMovementCommandPacketView& packet);

    [[nodiscard]] AuthoritativeMovementTickReport Tick(std::uint64_t serverTick,
                                                        float         dtSeconds);

    [[nodiscard]] std::optional<Vector3> GetActorPosition(EntityId entityId) const;
    [[nodiscard]] std::vector<EntityId>  ConsumeDirtyEntities();

private:
    AuthoritativeMovementInputAdapter m_InputAdapter;
};

} // namespace SagaEngine::ServerAuthority::Simulation
