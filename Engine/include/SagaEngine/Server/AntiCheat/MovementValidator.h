/// @file MovementValidator.h
/// @brief Server-side anti-cheat: movement validation and anomaly detection.
///
/// Layer  : SagaEngine / Server / AntiCheat
/// Purpose: Validates client-reported movement against server-authoritative
///          physics to detect speed hacks, teleport hacks, noclip, and other
///          common movement exploits. Uses statistical analysis over a sliding
///          window to avoid false positives from legitimate lag spikes.
///
/// Design:
///   - Server tracks last N movement samples per client.
///   - Each sample: position, velocity, acceleration, timestamp.
///   - Validation checks: speed limit, acceleration limit, teleport detection,
///     collision boundary check, frequency analysis.
///   - Anomaly scoring: each violation adds to a cumulative score; threshold
///     triggers a flag for operator review or automatic action.
///
/// Thread safety: NOT thread-safe. Call from server simulation thread only.

#pragma once

#include "SagaEngine/ECS/Entity.h"
#include "SagaEngine/Math/Vec3.h"

#include <array>
#include <chrono>
#include <cstdint>
#include <deque>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace SagaEngine::Server::AntiCheat {

// ─── Movement sample ────────────────────────────────────────────────────────

struct MovementSample
{
    Math::Vec3     position;
    Math::Vec3     velocity;
    Math::Vec3     acceleration;
    std::int64_t   timestampUs = 0;   ///< Microseconds (server receive time).
    std::uint32_t  tickNumber = 0;
};

// ─── Violation types ────────────────────────────────────────────────────────

enum class ViolationType : std::uint8_t
{
    None,
    SpeedHack,           ///< Exceeds maximum allowed speed.
    TeleportHack,        ///< Position jump beyond teleport threshold.
    Noclip,              ///< Passed through solid geometry.
    AccelerationHack,    ///< Exceeds maximum acceleration.
    FrequencyAnomaly,    ///< Unusual movement pattern (e.g. rapid direction changes).
    StuckInGeometry,     ///< Entity inside collision bounds.
    ImpossibleTrajectory ///< Trajectory violates physics constraints.
};

/// Human-readable violation name.
[[nodiscard]] const char* ViolationTypeToString(ViolationType type) noexcept;

// ─── Violation record ───────────────────────────────────────────────────────

struct ViolationRecord
{
    ViolationType   type = ViolationType::None;
    std::string     description;
    float           severity = 0.f;          ///< 0.0-1.0 normalized severity.
    std::int64_t    timestampUs = 0;
    std::uint32_t   tickNumber = 0;
};

// ─── Anomaly score ──────────────────────────────────────────────────────────

/// Cumulative anomaly score for a client. Decays over time.
struct AnomalyScore
{
    float       currentScore = 0.f;          ///< Current anomaly score (0-100).
    float       peakScore = 0.f;             ///< Highest score ever reached.
    std::int64_t lastViolationUs = 0;        ///< Time of last violation.
    std::uint32_t violationCount = 0;        ///< Total violations recorded.
    std::uint32_t falsePositiveFlags = 0;    ///< Operator-marked false positives.

    /// Add to the score with decay. Score decays by 1 point per decayIntervalSec.
    void Add(float points, float decayIntervalSec = 10.f, std::int64_t nowUs = 0);

    /// Get the decayed score at the current time.
    [[nodiscard]] float GetDecayedScore(std::int64_t nowUs,
                                         float decayIntervalSec = 10.f) const;

    /// Check if the score exceeds a threshold.
    [[nodiscard]] bool ExceedsThreshold(float threshold, std::int64_t nowUs,
                                         float decayIntervalSec = 10.f) const;
};

// ─── Validator config ───────────────────────────────────────────────────────

struct MovementValidatorConfig
{
    /// Maximum allowed speed in world units per second.
    float maxSpeed = 15.f;

    /// Maximum allowed acceleration in world units per second squared.
    float maxAcceleration = 50.f;

    /// Teleport detection threshold: position jump in one tick.
    float teleportThreshold = 100.f;

    /// Number of samples to keep in the sliding window.
    std::size_t sampleWindowSize = 64;

    /// Anomaly score threshold to flag a client (0-100 scale).
    float anomalyFlagThreshold = 50.f;

    /// Score decay interval in seconds (reduces score over time).
    float scoreDecayIntervalSec = 10.f;

    /// Score added for a speed hack violation.
    float speedHackScore = 15.f;

    /// Score added for a teleport hack violation.
    float teleportHackScore = 30.f;

    /// Score added for noclip detection.
    float noclipScore = 25.f;

    /// Score added for acceleration hack.
    float accelerationHackScore = 10.f;

    /// Score added for frequency anomaly.
    float frequencyAnomalyScore = 20.f;
};

// ─── Validation result ──────────────────────────────────────────────────────

struct ValidationResult
{
    bool                isSuspicious = false;
    ViolationRecord     violation;
    AnomalyScore        anomalyScore;
};

// ─── Movement Validator ─────────────────────────────────────────────────────

/// Per-client movement validator.
///
/// Tracks movement samples and validates each new position against
/// physics constraints and statistical anomaly detection.
class MovementValidator
{
public:
    MovementValidator() = default;
    explicit MovementValidator(MovementValidatorConfig config) noexcept;

    // ─── Sample recording ─────────────────────────────────────────────────────

    /// Record a new movement sample. Call each tick when client position updates.
    void RecordSample(ECS::EntityId entityId, const Math::Vec3& position,
                      std::int64_t timestampUs, std::uint32_t tickNumber);

    /// Record a sample with pre-computed velocity and acceleration.
    void RecordSampleFull(ECS::EntityId entityId, const MovementSample& sample);

    // ─── Validation ───────────────────────────────────────────────────────────

    /// Validate the latest movement sample. Returns result with any violations.
    [[nodiscard]] ValidationResult Validate(ECS::EntityId entityId,
                                           const Math::Vec3& reportedPosition,
                                           std::int64_t timestampUs,
                                           std::uint32_t tickNumber);

    /// Validate against collision geometry (requires external collision check).
    void ReportCollisionCheck(ECS::EntityId entityId, bool isInGeometry);

    // ─── State management ─────────────────────────────────────────────────────

    /// Reset validator state (e.g. on client reconnect).
    void Reset();

    /// Mark a violation as a false positive (for tuning).
    void MarkFalsePositive(std::uint32_t violationIndex);

    // ─── Diagnostics ──────────────────────────────────────────────────────────

    [[nodiscard]] const AnomalyScore& GetAnomalyScore() const noexcept { return anomalyScore_; }
    [[nodiscard]] const std::deque<MovementSample>& GetSamples() const noexcept { return samples_; }
    [[nodiscard]] const std::vector<ViolationRecord>& GetViolations() const noexcept { return violations_; }
    [[nodiscard]] std::size_t GetSampleCount() const noexcept { return samples_.size(); }
    [[nodiscard]] std::size_t GetViolationCount() const noexcept { return violations_.size(); }
    [[nodiscard]] const MovementValidatorConfig& GetConfig() const noexcept { return config_; }

private:
    MovementValidatorConfig config_;

    /// Sliding window of movement samples.
    std::deque<MovementSample> samples_;

    /// Record of all violations (bounded).
    std::vector<ViolationRecord> violations_;

    /// Cumulative anomaly score.
    AnomalyScore anomalyScore_;

    /// Last validated position (for delta computation).
    std::optional<Math::Vec3> lastPosition_;
    std::optional<MovementSample> lastSample_;

    // ─── Validation checks ────────────────────────────────────────────────────

    [[nodiscard]] std::optional<ViolationRecord> CheckSpeed(
        const Math::Vec3& currentPosition, std::int64_t timestampUs) const;

    [[nodiscard]] std::optional<ViolationRecord> CheckTeleport(
        const Math::Vec3& currentPosition, std::int64_t timestampUs) const;

    [[nodiscard]] std::optional<ViolationRecord> CheckAcceleration(
        const Math::Vec3& currentVelocity, std::int64_t timestampUs) const;

    [[nodiscard]] std::optional<ViolationRecord> CheckFrequencyAnomaly() const;

    // ─── Helpers ──────────────────────────────────────────────────────────────

    void RecordViolation(const ViolationRecord& violation);
    Math::Vec3 ComputeVelocity(const Math::Vec3& newPos, std::int64_t newTimestamp,
                                const Math::Vec3& oldPos, std::int64_t oldTimestamp) const;
};

// ─── Global anti-cheat manager ──────────────────────────────────────────────

/// Manages movement validators for all connected clients.
class AntiCheatManager
{
public:
    AntiCheatManager() = default;
    explicit AntiCheatManager(MovementValidatorConfig config) noexcept;

    /// Get or create a validator for a client entity.
    MovementValidator& GetValidator(ECS::EntityId entityId);

    /// Remove a client's validator (on disconnect).
    void RemoveClient(ECS::EntityId entityId);

    /// Validate all clients and return flagged ones.
    [[nodiscard]] std::vector<ECS::EntityId> ValidateAllAndFlag(
        const std::vector<std::pair<ECS::EntityId, Math::Vec3>>& clientPositions,
        std::int64_t timestampUs, std::uint32_t tickNumber);

    /// Get a list of all currently flagged clients (score above threshold).
    [[nodiscard]] std::vector<ECS::EntityId> GetFlaggedClients(
        std::int64_t timestampUs) const;

    /// Get the validator for a client, or nullptr if none exists.
    [[nodiscard]] const MovementValidator* GetValidatorPtr(
        ECS::EntityId entityId) const noexcept;

private:
    MovementValidatorConfig config_;
    std::unordered_map<ECS::EntityId, MovementValidator> validators_;
};

} // namespace SagaEngine::Server::AntiCheat
