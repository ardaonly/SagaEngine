/// @file AuthoritativeMovementCore.cpp
/// @brief AuthoritativeMovementCore implementation.

#include "SagaEngine/ServerAuthority/Simulation/AuthoritativeMovementCore.h"

#include <algorithm>
#include <cmath>

namespace SagaEngine::ServerAuthority::Simulation
{

namespace
{

[[nodiscard]] float InputMagnitudeSquared(const InputCommand& command) noexcept
{
    return command.moveX * command.moveX +
           command.moveY * command.moveY +
           command.moveZ * command.moveZ;
}

[[nodiscard]] bool IsMutatingValidatorDecision(MovementDecision decision) noexcept
{
    return decision == MovementDecision::Accept ||
           decision == MovementDecision::AcceptSoft ||
           decision == MovementDecision::Clamp;
}

[[nodiscard]] AuthoritativeMovementDecision ToAuthorityDecision(MovementDecision decision) noexcept
{
    if (decision == MovementDecision::Clamp)
        return AuthoritativeMovementDecision::Clamped;

    if (decision == MovementDecision::Accept ||
        decision == MovementDecision::AcceptSoft)
    {
        return AuthoritativeMovementDecision::Accepted;
    }

    return AuthoritativeMovementDecision::RejectedByValidator;
}

[[nodiscard]] bool PositionChanged(const Vector3& a, const Vector3& b) noexcept
{
    return a.x != b.x || a.y != b.y || a.z != b.z;
}

} // anonymous namespace

AuthoritativeMovementCore::AuthoritativeMovementCore(AuthoritativeMovementCoreConfig config)
    : m_Config(config)
    , m_InputRouter(m_Config.inputQueue)
    , m_MovementValidator(m_Config.movementValidator)
{
    if (m_Config.movementUnitsPerSecond < 0.0f)
        m_Config.movementUnitsPerSecond = 0.0f;

    if (m_Config.maxInputMagnitude < 0.0f)
        m_Config.maxInputMagnitude = 0.0f;

    if (m_Config.maxCommandsPerClientPerTick == 0)
        m_Config.maxCommandsPerClientPerTick = 1;
}

bool AuthoritativeMovementCore::RegisterActor(ClientId clientId,
                                              EntityId entityId,
                                              Vector3  initialPosition)
{
    std::lock_guard<std::mutex> lock(m_Mutex);

    const auto actorIt = m_Actors.find(entityId);
    if (actorIt != m_Actors.end() && actorIt->second.owner != clientId)
        return false;

    const auto clientIt = m_ClientActors.find(clientId);
    if (clientIt != m_ClientActors.end() && clientIt->second != entityId)
    {
        m_Actors.erase(clientIt->second);
        m_MovementValidator.Untrack(clientIt->second);
    }

    const bool hadClient = m_InputRouter.HasClient(clientId);
    if (!hadClient)
    {
        m_InputRouter.AddClient(clientId);
        m_ClientOrder.push_back(clientId);
    }

    m_ClientActors[clientId] = entityId;
    m_Actors[entityId] = ActorState{clientId, initialPosition};
    m_MovementValidator.Track(entityId, initialPosition);
    return true;
}

void AuthoritativeMovementCore::UnregisterActor(ClientId clientId)
{
    std::lock_guard<std::mutex> lock(m_Mutex);

    const auto clientIt = m_ClientActors.find(clientId);
    if (clientIt != m_ClientActors.end())
    {
        const EntityId entityId = clientIt->second;
        m_Actors.erase(entityId);
        m_MovementValidator.Untrack(entityId);
        m_DirtyEntitySet.erase(entityId);
        m_DirtyEntities.erase(
            std::remove(m_DirtyEntities.begin(), m_DirtyEntities.end(), entityId),
            m_DirtyEntities.end());
        m_ClientActors.erase(clientIt);
    }

    m_InputRouter.RemoveClient(clientId);
    m_ClientOrder.erase(
        std::remove(m_ClientOrder.begin(), m_ClientOrder.end(), clientId),
        m_ClientOrder.end());
}

AuthoritativeMovementResult AuthoritativeMovementCore::EnqueueInput(
    ClientId            clientId,
    EntityId            entityId,
    const InputCommand& command)
{
    std::lock_guard<std::mutex> lock(m_Mutex);

    const auto clientIt = m_ClientActors.find(clientId);
    if (clientIt == m_ClientActors.end())
        return MakeResult(clientId, entityId, command,
                          AuthoritativeMovementDecision::RejectUnknownClient, {});

    const EntityId ownedEntity = clientIt->second;
    const auto actorIt = m_Actors.find(ownedEntity);
    const Vector3 current = actorIt != m_Actors.end() ? actorIt->second.position : Vector3{};

    if (ownedEntity != entityId || actorIt == m_Actors.end())
        return MakeResult(clientId, entityId, command,
                          AuthoritativeMovementDecision::RejectOwnership, current);

    if (!IsInputFinite(command) || !IsInputWithinMagnitude(command))
        return MakeResult(clientId, entityId, command,
                          AuthoritativeMovementDecision::RejectInput, current);

    if (!m_InputRouter.Enqueue(clientId, command))
        return MakeResult(clientId, entityId, command,
                          AuthoritativeMovementDecision::RejectQueue, current);

    return MakeResult(clientId, entityId, command,
                      AuthoritativeMovementDecision::Queued, current);
}

AuthoritativeMovementTickReport AuthoritativeMovementCore::Tick(std::uint64_t serverTick,
                                                                 float         dtSeconds)
{
    std::lock_guard<std::mutex> lock(m_Mutex);

    AuthoritativeMovementTickReport report;

    for (ClientId clientId : m_ClientOrder)
    {
        const auto clientIt = m_ClientActors.find(clientId);
        if (clientIt == m_ClientActors.end())
            continue;

        const EntityId entityId = clientIt->second;

        for (std::size_t i = 0; i < m_Config.maxCommandsPerClientPerTick; ++i)
        {
            std::optional<InputCommand> command = m_InputRouter.Dequeue(clientId, serverTick);
            if (!command)
                break;

            auto actorIt = m_Actors.find(entityId);
            if (actorIt == m_Actors.end())
            {
                report.results.push_back(MakeResult(
                    clientId, entityId, *command,
                    AuthoritativeMovementDecision::RejectOwnership, {}));
                continue;
            }

            const Vector3 previous = actorIt->second.position;
            const Vector3 proposed = ComputeProposedPosition(previous, *command, dtSeconds);

            AuthoritativeMovementResult result;
            result.clientId = clientId;
            result.entityId = entityId;
            result.inputSequence = command->sequence;
            result.serverTick = serverTick;
            result.previousPosition = previous;
            result.proposedPosition = proposed;
            result.finalPosition = previous;

            MovementVerdict verdict;
            if (dtSeconds <= 0.0f || !std::isfinite(dtSeconds))
            {
                verdict.decision = MovementDecision::RejectStale;
                verdict.correctedPosition = previous;
            }
            else
            {
                verdict = m_MovementValidator.Evaluate(
                    entityId, proposed, serverTick, dtSeconds);
            }

            result.validatorDecision = verdict.decision;
            result.decision = ToAuthorityDecision(verdict.decision);

            if (IsMutatingValidatorDecision(verdict.decision))
            {
                actorIt->second.position = verdict.correctedPosition;
                result.finalPosition = verdict.correctedPosition;
                result.mutated = PositionChanged(previous, verdict.correctedPosition);
                result.dirty = result.mutated;
                if (result.dirty)
                    MarkDirtyLocked(entityId, report);
            }

            report.results.push_back(result);
        }
    }

    return report;
}

std::optional<Vector3> AuthoritativeMovementCore::GetActorPosition(EntityId entityId) const
{
    std::lock_guard<std::mutex> lock(m_Mutex);
    const auto it = m_Actors.find(entityId);
    if (it == m_Actors.end())
        return std::nullopt;

    return it->second.position;
}

std::vector<EntityId> AuthoritativeMovementCore::ConsumeDirtyEntities()
{
    std::lock_guard<std::mutex> lock(m_Mutex);

    std::vector<EntityId> out = std::move(m_DirtyEntities);
    m_DirtyEntities.clear();
    m_DirtyEntitySet.clear();
    return out;
}

AuthoritativeMovementResult AuthoritativeMovementCore::MakeResult(
    ClientId                      clientId,
    EntityId                      entityId,
    const InputCommand&           command,
    AuthoritativeMovementDecision decision,
    const Vector3&                position) const
{
    AuthoritativeMovementResult result;
    result.clientId = clientId;
    result.entityId = entityId;
    result.inputSequence = command.sequence;
    result.serverTick = command.serverRecvTick;
    result.decision = decision;
    result.previousPosition = position;
    result.proposedPosition = position;
    result.finalPosition = position;
    return result;
}

bool AuthoritativeMovementCore::IsInputFinite(const InputCommand& command) const noexcept
{
    return std::isfinite(command.moveX) &&
           std::isfinite(command.moveY) &&
           std::isfinite(command.moveZ) &&
           std::isfinite(command.lookYaw) &&
           std::isfinite(command.lookPitch);
}

bool AuthoritativeMovementCore::IsInputWithinMagnitude(const InputCommand& command) const noexcept
{
    const float maxMagnitude = m_Config.maxInputMagnitude;
    const float magnitudeSquared = InputMagnitudeSquared(command);
    return magnitudeSquared <= maxMagnitude * maxMagnitude;
}

Vector3 AuthoritativeMovementCore::ComputeProposedPosition(const Vector3&      current,
                                                           const InputCommand& command,
                                                           float               dtSeconds) const noexcept
{
    if (dtSeconds <= 0.0f || !std::isfinite(dtSeconds))
        return current;

    const float magnitudeSquared = InputMagnitudeSquared(command);
    if (magnitudeSquared <= 0.000001f)
        return current;

    const float invMagnitude = 1.0f / std::sqrt(magnitudeSquared);
    const float distance = m_Config.movementUnitsPerSecond * dtSeconds;

    return Vector3{
        current.x + command.moveX * invMagnitude * distance,
        current.y + command.moveY * invMagnitude * distance,
        current.z + command.moveZ * invMagnitude * distance
    };
}

void AuthoritativeMovementCore::MarkDirtyLocked(EntityId entityId,
                                                AuthoritativeMovementTickReport& report)
{
    report.dirtyEntityIds.push_back(entityId);

    if (m_DirtyEntitySet.insert(entityId).second)
        m_DirtyEntities.push_back(entityId);
}

} // namespace SagaEngine::ServerAuthority::Simulation
