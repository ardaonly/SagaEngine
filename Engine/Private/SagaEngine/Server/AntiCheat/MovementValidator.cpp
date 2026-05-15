/// @file MovementValidator.cpp
/// @brief Server-side anti-cheat movement validation implementation.

#include "SagaEngine/Server/AntiCheat/MovementValidator.h"
#include "SagaEngine/Core/Log/Log.h"
#include "SagaEngine/Core/Profiling/Profiler.h"

#include <algorithm>
#include <cmath>
#include <sstream>

namespace SagaEngine::Server::AntiCheat {

static constexpr const char* kTag = "AntiCheat";

// ─── Violation type names ─────────────────────────────────────────────────────

const char* ViolationTypeToString(ViolationType type) noexcept
{
    switch (type)
    {
    case ViolationType::SpeedHack:          return "SpeedHack";
    case ViolationType::TeleportHack:       return "TeleportHack";
    case ViolationType::Noclip:             return "Noclip";
    case ViolationType::AccelerationHack:   return "AccelerationHack";
    case ViolationType::FrequencyAnomaly:   return "FrequencyAnomaly";
    case ViolationType::StuckInGeometry:    return "StuckInGeometry";
    case ViolationType::ImpossibleTrajectory: return "ImpossibleTrajectory";
    default:                                return "None";
    }
}

// ─── Anomaly score ────────────────────────────────────────────────────────────

void AnomalyScore::Add(float points, float decayIntervalSec, std::int64_t nowUs)
{
    // Apply decay first
    if (lastViolationUs > 0 && nowUs > 0)
    {
        const float elapsedSec = static_cast<float>(nowUs - lastViolationUs) / 1'000'000.f;
        const float decaySteps = elapsedSec / decayIntervalSec;
        currentScore = std::max(0.f, currentScore - decaySteps);
    }

    currentScore += points;
    peakScore = std::max(peakScore, currentScore);
    lastViolationUs = nowUs;
    violationCount++;
}

float AnomalyScore::GetDecayedScore(std::int64_t nowUs, float decayIntervalSec) const
{
    if (lastViolationUs == 0 || nowUs == 0)
        return currentScore;

    const float elapsedSec = static_cast<float>(nowUs - lastViolationUs) / 1'000'000.f;
    const float decaySteps = elapsedSec / decayIntervalSec;
    return std::max(0.f, currentScore - decaySteps);
}

bool AnomalyScore::ExceedsThreshold(float threshold, std::int64_t nowUs,
                                      float decayIntervalSec) const
{
    return GetDecayedScore(nowUs, decayIntervalSec) > threshold;
}

// ─── Movement validator ───────────────────────────────────────────────────────

MovementValidator::MovementValidator(MovementValidatorConfig config) noexcept
    : config_(config)
{
    samples_.clear();
    violations_.clear();
}

void MovementValidator::RecordSample(ECS::EntityId entityId, const Math::Vec3& position,
                                      std::int64_t timestampUs, std::uint32_t tickNumber)
{
    SAGA_PROFILE_SCOPE("MovementValidator::RecordSample");

    MovementSample sample;
    sample.position = position;
    sample.timestampUs = timestampUs;
    sample.tickNumber = tickNumber;

    // Compute velocity from last sample
    if (lastPosition_.has_value() && lastSample_.has_value())
    {
        sample.velocity = ComputeVelocity(position, timestampUs,
                                           lastPosition_.value(), lastSample_->timestampUs);
        sample.acceleration = (sample.velocity - lastSample_->velocity) *
                               (1'000'000.f / static_cast<float>(
                                   std::max<std::int64_t>(1, timestampUs - lastSample_->timestampUs)));
    }

    samples_.push_back(sample);

    // Enforce sliding window
    if (samples_.size() > config_.sampleWindowSize)
    {
        samples_.pop_front();
    }

    lastPosition_ = position;
    lastSample_ = sample;

    (void)entityId;
}

void MovementValidator::RecordSampleFull(ECS::EntityId entityId,
                                          const MovementSample& sample)
{
    SAGA_PROFILE_SCOPE("MovementValidator::RecordSampleFull");

    samples_.push_back(sample);

    if (samples_.size() > config_.sampleWindowSize)
    {
        samples_.pop_front();
    }

    lastPosition_ = sample.position;
    lastSample_ = sample;

    (void)entityId;
}

ValidationResult MovementValidator::Validate(ECS::EntityId entityId,
                                              const Math::Vec3& reportedPosition,
                                              std::int64_t timestampUs,
                                              std::uint32_t tickNumber)
{
    SAGA_PROFILE_SCOPE("MovementValidator::Validate");

    ValidationResult result;

    // Check speed
    if (auto violation = CheckSpeed(reportedPosition, timestampUs))
    {
        result.isSuspicious = true;
        result.violation = violation.value();
        RecordViolation(result.violation);
        anomalyScore_.Add(config_.speedHackScore, config_.scoreDecayIntervalSec, timestampUs);
        return result;
    }

    // Check teleport
    if (auto violation = CheckTeleport(reportedPosition, timestampUs))
    {
        result.isSuspicious = true;
        result.violation = violation.value();
        RecordViolation(result.violation);
        anomalyScore_.Add(config_.teleportHackScore, config_.scoreDecayIntervalSec, timestampUs);
        return result;
    }

    // Record the sample for future checks
    RecordSample(entityId, reportedPosition, timestampUs, tickNumber);

    result.anomalyScore = anomalyScore_;
    return result;
}

void MovementValidator::ReportCollisionCheck(ECS::EntityId entityId, bool isInGeometry)
{
    if (isInGeometry && lastPosition_.has_value())
    {
        ViolationRecord violation;
        violation.type = ViolationType::StuckInGeometry;
        violation.description = "Entity detected inside collision geometry";
        violation.severity = 0.5f;
        violation.timestampUs = samples_.empty() ? 0 : samples_.back().timestampUs;
        violation.tickNumber = samples_.empty() ? 0 : samples_.back().tickNumber;

        RecordViolation(violation);
        anomalyScore_.Add(config_.noclipScore, config_.scoreDecayIntervalSec,
                          violation.timestampUs);
    }

    (void)entityId;
}

void MovementValidator::Reset()
{
    samples_.clear();
    violations_.clear();
    lastPosition_.reset();
    lastSample_.reset();
}

void MovementValidator::MarkFalsePositive(std::uint32_t violationIndex)
{
    if (violationIndex < violations_.size())
    {
        anomalyScore_.falsePositiveFlags++;
    }
}

// ─── Validation checks ────────────────────────────────────────────────────────

std::optional<ViolationRecord> MovementValidator::CheckSpeed(
    const Math::Vec3& currentPosition, std::int64_t timestampUs) const
{
    if (!lastPosition_.has_value() || samples_.empty())
        return std::nullopt;

    const float distance = currentPosition.DistanceTo(lastPosition_.value());
    const float elapsedSec = static_cast<float>(timestampUs - samples_.back().timestampUs) /
                              1'000'000.f;

    if (elapsedSec <= 0.f)
        return std::nullopt;

    const float speed = distance / elapsedSec;

    if (speed > config_.maxSpeed)
    {
        ViolationRecord violation;
        violation.type = ViolationType::SpeedHack;
        violation.severity = std::min(1.f, (speed - config_.maxSpeed) / config_.maxSpeed);

        std::ostringstream oss;
        oss << "Speed " << speed << " exceeds max " << config_.maxSpeed
            << " (+" << (speed - config_.maxSpeed) << ")";
        violation.description = oss.str();
        violation.timestampUs = timestampUs;

        return violation;
    }

    return std::nullopt;
}

std::optional<ViolationRecord> MovementValidator::CheckTeleport(
    const Math::Vec3& currentPosition, std::int64_t timestampUs) const
{
    if (!lastPosition_.has_value())
        return std::nullopt;

    const float distance = currentPosition.DistanceTo(lastPosition_.value());

    if (distance > config_.teleportThreshold)
    {
        ViolationRecord violation;
        violation.type = ViolationType::TeleportHack;
        violation.severity = std::min(1.f, distance / (config_.teleportThreshold * 2.f));

        std::ostringstream oss;
        oss << "Position jump " << distance << " exceeds teleport threshold "
            << config_.teleportThreshold;
        violation.description = oss.str();
        violation.timestampUs = timestampUs;

        return violation;
    }

    return std::nullopt;
}

std::optional<ViolationRecord> MovementValidator::CheckAcceleration(
    const Math::Vec3& currentVelocity, std::int64_t timestampUs) const
{
    if (!lastSample_.has_value())
        return std::nullopt;

    const Math::Vec3 accelVec = currentVelocity - lastSample_->velocity;
    const float accelMag = accelVec.Length();
    const float elapsedSec = static_cast<float>(
        timestampUs - lastSample_->timestampUs) / 1'000'000.f;

    if (elapsedSec <= 0.f)
        return std::nullopt;

    const float acceleration = accelMag / elapsedSec;

    if (acceleration > config_.maxAcceleration)
    {
        ViolationRecord violation;
        violation.type = ViolationType::AccelerationHack;
        violation.severity = std::min(1.f,
            (acceleration - config_.maxAcceleration) / config_.maxAcceleration);

        std::ostringstream oss;
        oss << "Acceleration " << acceleration << " exceeds max "
            << config_.maxAcceleration;
        violation.description = oss.str();
        violation.timestampUs = timestampUs;

        return violation;
    }

    return std::nullopt;
}

std::optional<ViolationRecord> MovementValidator::CheckFrequencyAnomaly() const
{
    // Check for rapid direction changes (zigzag pattern indicative of exploits)
    if (samples_.size() < 8)
        return std::nullopt;

    std::uint32_t directionChanges = 0;
    for (size_t i = 2; i < samples_.size(); ++i)
    {
        const auto& prev = samples_[i - 1];
        const auto& curr = samples_[i];
        const auto& prevPrev = samples_[i - 2];

        const Math::Vec3 dir1 = prev.velocity - prevPrev.velocity;
        const Math::Vec3 dir2 = curr.velocity - prev.velocity;

        // Check for >90 degree direction change
        if (dir1.Dot(dir2) < 0.f)
        {
            directionChanges++;
        }
    }

    // More than 50% direction changes is suspicious
    const float changeRate = static_cast<float>(directionChanges) /
                             static_cast<float>(samples_.size() - 2);

    if (changeRate > 0.5f)
    {
        ViolationRecord violation;
        violation.type = ViolationType::FrequencyAnomaly;
        violation.severity = changeRate;
        violation.description = "Rapid direction changes detected";
        violation.timestampUs = samples_.back().timestampUs;
        violation.tickNumber = samples_.back().tickNumber;

        return violation;
    }

    return std::nullopt;
}

// ─── Helpers ──────────────────────────────────────────────────────────────────

void MovementValidator::RecordViolation(const ViolationRecord& violation)
{
    violations_.push_back(violation);

    // Bound the violation history (keep last 128)
    if (violations_.size() > 128)
    {
        violations_.erase(violations_.begin());
    }
}

Math::Vec3 MovementValidator::ComputeVelocity(const Math::Vec3& newPos,
                                                std::int64_t newTimestamp,
                                                const Math::Vec3& oldPos,
                                                std::int64_t oldTimestamp) const
{
    const float elapsedSec = static_cast<float>(newTimestamp - oldTimestamp) / 1'000'000.f;
    if (elapsedSec <= 0.f)
        return Math::Vec3::Zero();

    return (newPos - oldPos) / elapsedSec;
}

// ─── Anti-Cheat Manager ───────────────────────────────────────────────────────

AntiCheatManager::AntiCheatManager(MovementValidatorConfig config) noexcept
    : config_(config)
{
}

MovementValidator& AntiCheatManager::GetValidator(ECS::EntityId entityId)
{
    auto it = validators_.find(entityId);
    if (it == validators_.end())
    {
        it = validators_.emplace(entityId, MovementValidator{config_}).first;
    }
    return it->second;
}

void AntiCheatManager::RemoveClient(ECS::EntityId entityId)
{
    validators_.erase(entityId);
}

std::vector<ECS::EntityId> AntiCheatManager::ValidateAllAndFlag(
    const std::vector<std::pair<ECS::EntityId, Math::Vec3>>& clientPositions,
    std::int64_t timestampUs, std::uint32_t tickNumber)
{
    SAGA_PROFILE_SCOPE("AntiCheatManager::ValidateAllAndFlag");

    std::vector<ECS::EntityId> flaggedClients;

    for (const auto& [entityId, position] : clientPositions)
    {
        auto& validator = GetValidator(entityId);
        const auto result = validator.Validate(entityId, position, timestampUs, tickNumber);

        if (result.isSuspicious ||
            validator.GetAnomalyScore().ExceedsThreshold(
                config_.anomalyFlagThreshold, timestampUs, config_.scoreDecayIntervalSec))
        {
            flaggedClients.push_back(entityId);
        }
    }

    return flaggedClients;
}

std::vector<ECS::EntityId> AntiCheatManager::GetFlaggedClients(
    std::int64_t timestampUs) const
{
    std::vector<ECS::EntityId> flagged;

    for (const auto& [entityId, validator] : validators_)
    {
        if (validator.GetAnomalyScore().ExceedsThreshold(
                config_.anomalyFlagThreshold, timestampUs, config_.scoreDecayIntervalSec))
        {
            flagged.push_back(entityId);
        }
    }

    return flagged;
}

const MovementValidator* AntiCheatManager::GetValidatorPtr(
    ECS::EntityId entityId) const noexcept
{
    auto it = validators_.find(entityId);
    if (it != validators_.end())
        return &it->second;
    return nullptr;
}

} // namespace SagaEngine::Server::AntiCheat
