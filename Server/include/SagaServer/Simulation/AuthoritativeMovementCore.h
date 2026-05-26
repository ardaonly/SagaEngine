/// @file AuthoritativeMovementCore.h
/// @brief Narrow server-owned movement authority core for fixed-tick validation.

#pragma once

#include "SagaServer/Simulation/InputCommandQueue.h"
#include "SagaServer/Simulation/MovementValidator.h"

#include <cstdint>
#include <mutex>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace SagaEngine::Server::Simulation
{

/// Configuration for the authoritative movement proof core.
struct AuthoritativeMovementCoreConfig
{
    InputQueueConfig          inputQueue;
    MovementValidatorConfig   movementValidator;
    float                     movementUnitsPerSecond{6.0f};
    float                     maxInputMagnitude{1.0f};
    std::size_t               maxCommandsPerClientPerTick{4};
};

/// High-level decision emitted by AuthoritativeMovementCore.
enum class AuthoritativeMovementDecision : std::uint8_t
{
    Queued = 0,
    RejectUnknownClient,
    RejectOwnership,
    RejectInput,
    RejectQueue,
    Accepted,
    Clamped,
    RejectedByValidator
};

/// Observable result for enqueue rejection/acceptance or tick-time mutation.
struct AuthoritativeMovementResult
{
    ClientId                      clientId{0};
    EntityId                      entityId{0};
    std::uint64_t                 inputSequence{0};
    std::uint64_t                 serverTick{0};
    AuthoritativeMovementDecision decision{AuthoritativeMovementDecision::Queued};
    Vector3                       previousPosition{};
    Vector3                       proposedPosition{};
    Vector3                       finalPosition{};
    bool                          mutated{false};
    bool                          dirty{false};
    MovementDecision              validatorDecision{MovementDecision::Accept};
};

/// Per-tick output in the exact order commands were evaluated.
struct AuthoritativeMovementTickReport
{
    std::vector<AuthoritativeMovementResult> results;
    std::vector<EntityId>                    dirtyEntityIds;
};

/// Server-owned movement state for client-controlled actors.
class AuthoritativeMovementCore final
{
public:
    explicit AuthoritativeMovementCore(AuthoritativeMovementCoreConfig config = {});
    ~AuthoritativeMovementCore() = default;

    AuthoritativeMovementCore(const AuthoritativeMovementCore&)            = delete;
    AuthoritativeMovementCore& operator=(const AuthoritativeMovementCore&) = delete;

    [[nodiscard]] bool RegisterActor(ClientId clientId,
                                     EntityId entityId,
                                     Vector3  initialPosition);
    void UnregisterActor(ClientId clientId);

    [[nodiscard]] AuthoritativeMovementResult EnqueueInput(ClientId            clientId,
                                                            EntityId            entityId,
                                                            const InputCommand& command);

    [[nodiscard]] AuthoritativeMovementTickReport Tick(std::uint64_t serverTick,
                                                        float         dtSeconds);

    [[nodiscard]] std::optional<Vector3> GetActorPosition(EntityId entityId) const;
    [[nodiscard]] std::vector<EntityId>  ConsumeDirtyEntities();

private:
    struct ActorState
    {
        ClientId owner{0};
        Vector3  position{};
    };

    [[nodiscard]] AuthoritativeMovementResult MakeResult(
        ClientId                      clientId,
        EntityId                      entityId,
        const InputCommand&           command,
        AuthoritativeMovementDecision decision,
        const Vector3&                position) const;

    [[nodiscard]] bool IsInputFinite(const InputCommand& command) const noexcept;
    [[nodiscard]] bool IsInputWithinMagnitude(const InputCommand& command) const noexcept;
    [[nodiscard]] Vector3 ComputeProposedPosition(const Vector3&      current,
                                                   const InputCommand& command,
                                                   float               dtSeconds) const noexcept;
    void MarkDirtyLocked(EntityId entityId, AuthoritativeMovementTickReport& report);

    AuthoritativeMovementCoreConfig m_Config;
    InputCommandRouter              m_InputRouter;
    MovementValidator               m_MovementValidator;

    mutable std::mutex              m_Mutex;
    std::unordered_map<ClientId, EntityId> m_ClientActors;
    std::unordered_map<EntityId, ActorState> m_Actors;
    std::vector<ClientId>           m_ClientOrder;
    std::vector<EntityId>           m_DirtyEntities;
    std::unordered_set<EntityId>    m_DirtyEntitySet;
};

} // namespace SagaEngine::Server::Simulation
