/// @file MovementValidator.h
/// @brief Server-side movement anti-cheat — speed, teleport, and trajectory gating.
///
/// Layer  : SagaServer / Simulation
/// Purpose: Client-predicted movement must be validated by the authoritative
///          server before being accepted into the simulation. MovementValidator
///          is a pure function object that remembers the last trusted state of
///          each tracked entity and evaluates every subsequent position update
///          against a configurable kinematics model:
///            - Maximum horizontal and vertical speed
///            - Maximum allowed delta per server tick (teleport gate)
///            - Acceleration ceiling (impossible burst detection)
///            - Per-entity tolerance budget that decays over time
///
/// The validator does not mutate world state directly. It returns a result
/// describing whether the update is accepted, corrected, or rejected, and the
/// caller is responsible for applying the decision.

#pragma once

#include "SagaServer/Networking/Core/NetworkTypes.h"
#include "SagaServer/Networking/Interest/InterestArea.h"
#include "SagaEngine/ECS/Entity.h"

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <optional>
#include <unordered_map>

namespace SagaEngine::Server::Simulation
{

using EntityId = ECS::EntityId;
using Vector3  = ::SagaEngine::Networking::Interest::Vector3;

// ─── Result Codes ─────────────────────────────────────────────────────────────

/// Outcome of a single MovementValidator::Evaluate call.
enum class MovementDecision : std::uint8_t
{
    Accept         = 0, ///< Update is valid and should be applied verbatim.
    AcceptSoft     = 1, ///< Update is plausible but eats tolerance budget.
    Clamp          = 2, ///< Update exceeded limits — caller should clamp to MaxReachable.
    RejectTeleport = 3, ///< Update is an impossible jump — caller should discard and correct.
    RejectSpeed    = 4, ///< Horizontal/vertical speed limit exceeded.
    RejectAccel    = 5, ///< Acceleration ceiling exceeded.
    RejectStale    = 6  ///< Timestamp regression or zero delta time.
};

/// Verdict returned by Evaluate, including correction metadata.
struct MovementVerdict
{
    MovementDecision decision{MovementDecision::Accept};
    Vector3          correctedPosition;
    float            observedHorizontalSpeed{0.0f};
    float            observedVerticalSpeed{0.0f};
    float            observedAcceleration{0.0f};
    float            toleranceRemaining{0.0f};
    std::uint32_t    violationsCount{0};
};

// ─── Configuration ────────────────────────────────────────────────────────────

/// Per-instance limits applied uniformly to every tracked entity.
struct MovementValidatorConfig
{
    float maxHorizontalSpeed{12.0f};   ///< Metres per second.
    float maxVerticalSpeed{20.0f};
    float maxAcceleration{40.0f};      ///< Metres per second squared.
    float teleportDistance{8.0f};      ///< Max single-tick delta in metres.
    float toleranceBudget{1.0f};       ///< Soft-failures allowed before hard rejection.
    float toleranceRegenPerSec{0.25f}; ///< Budget recovery rate while compliant.
    std::uint32_t banThreshold{20};    ///< Violations before Ban() returns true.
};

// ─── MovementValidator ────────────────────────────────────────────────────────

/// Authoritative movement gate for a pool of entities.
///
/// Threading: thread-safe. Every method takes a single internal mutex; the
/// hot path does not allocate once the per-entity record has been created.
class MovementValidator final
{
public:
    explicit MovementValidator(MovementValidatorConfig config = {});
    ~MovementValidator() = default;

    MovementValidator(const MovementValidator&)            = delete;
    MovementValidator& operator=(const MovementValidator&) = delete;

    /// Called once when the server gains authority over an entity.
    void Track(EntityId entity, const Vector3& initialPosition);

    /// Called when the server releases authority.
    void Untrack(EntityId entity);

    /// Evaluate a proposed position update. The verdict describes whether the
    /// caller should accept the update, clamp it, or reject and re-sync.
    [[nodiscard]] MovementVerdict Evaluate(EntityId       entity,
                                            const Vector3& proposedPosition,
                                            std::uint64_t  serverTick,
                                            float          dtSeconds);

    /// Force an authoritative position resync — useful after rollback or when
    /// recovering from a rejected update.
    void ForceResync(EntityId entity, const Vector3& position, std::uint64_t serverTick);

    /// Reset the tolerance budget and violation counter for `entity`.
    void ResetViolations(EntityId entity);

    /// Returns true when `entity` has exceeded `banThreshold` violations.
    [[nodiscard]] bool ShouldBan(EntityId entity) const;

    [[nodiscard]] std::optional<Vector3>    GetLastKnownPosition(EntityId entity) const;
    [[nodiscard]] std::uint32_t              GetViolations(EntityId entity) const;
    [[nodiscard]] std::size_t                GetTrackedCount() const;
    [[nodiscard]] const MovementValidatorConfig& GetConfig() const noexcept { return m_Config; }

private:
    // ── Internal state ───────────────────────────────────────────────────────

    struct EntityRecord
    {
        Vector3       lastPosition;
        float         lastHorizontalSpeed{0.0f};
        float         lastVerticalSpeed{0.0f};
        float         tolerance{0.0f};
        std::uint64_t lastTick{0};
        std::uint64_t lastUpdateUnixMs{0};
        std::uint32_t violations{0};
    };

    MovementValidatorConfig m_Config;

    mutable std::mutex m_Mutex;
    std::unordered_map<EntityId, EntityRecord> m_Records;
};

} // namespace SagaEngine::Server::Simulation
