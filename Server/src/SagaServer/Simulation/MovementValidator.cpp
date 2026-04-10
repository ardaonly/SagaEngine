/// @file MovementValidator.cpp
/// @brief MovementValidator implementation — server-authoritative movement gate.

#include "SagaServer/Simulation/MovementValidator.h"
#include "SagaEngine/Core/Log/Log.h"

#include <algorithm>
#include <chrono>
#include <cmath>

namespace SagaEngine::Server::Simulation
{

static constexpr const char* kTag = "MovementValidator";

namespace
{

[[nodiscard]] std::uint64_t NowUnixMs() noexcept
{
    using clock = std::chrono::system_clock;
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            clock::now().time_since_epoch()).count());
}

[[nodiscard]] float HorizontalDistance(const Vector3& a, const Vector3& b) noexcept
{
    const float dx = a.x - b.x;
    const float dz = a.z - b.z;
    return std::sqrt(dx * dx + dz * dz);
}

[[nodiscard]] Vector3 ClampTowards(const Vector3& from,
                                    const Vector3& towards,
                                    float          maxHorizontal,
                                    float          maxVertical) noexcept
{
    const float horiz = HorizontalDistance(from, towards);
    const float verticalDelta = towards.y - from.y;

    Vector3 out = from;

    if (horiz > maxHorizontal && horiz > 0.0001f)
    {
        const float scale = maxHorizontal / horiz;
        out.x = from.x + (towards.x - from.x) * scale;
        out.z = from.z + (towards.z - from.z) * scale;
    }
    else
    {
        out.x = towards.x;
        out.z = towards.z;
    }

    if (std::fabs(verticalDelta) > maxVertical)
    {
        out.y = from.y + (verticalDelta > 0.0f ? maxVertical : -maxVertical);
    }
    else
    {
        out.y = towards.y;
    }

    return out;
}

} // anonymous namespace

// ─── Construction ─────────────────────────────────────────────────────────────

MovementValidator::MovementValidator(MovementValidatorConfig config)
    : m_Config(config)
{
    if (m_Config.toleranceBudget < 0.0f)
        m_Config.toleranceBudget = 0.0f;
}

// ─── Tracking Lifecycle ───────────────────────────────────────────────────────

void MovementValidator::Track(EntityId entity, const Vector3& initialPosition)
{
    std::lock_guard<std::mutex> lock(m_Mutex);

    auto& rec = m_Records[entity];
    rec.lastPosition         = initialPosition;
    rec.lastHorizontalSpeed  = 0.0f;
    rec.lastVerticalSpeed    = 0.0f;
    rec.tolerance            = m_Config.toleranceBudget;
    rec.lastTick             = 0;
    rec.lastUpdateUnixMs     = NowUnixMs();
    rec.violations           = 0;
}

void MovementValidator::Untrack(EntityId entity)
{
    std::lock_guard<std::mutex> lock(m_Mutex);
    m_Records.erase(entity);
}

void MovementValidator::ForceResync(EntityId       entity,
                                     const Vector3& position,
                                     std::uint64_t  serverTick)
{
    std::lock_guard<std::mutex> lock(m_Mutex);

    auto& rec = m_Records[entity];
    rec.lastPosition        = position;
    rec.lastHorizontalSpeed = 0.0f;
    rec.lastVerticalSpeed   = 0.0f;
    rec.lastTick            = serverTick;
    rec.lastUpdateUnixMs    = NowUnixMs();
}

void MovementValidator::ResetViolations(EntityId entity)
{
    std::lock_guard<std::mutex> lock(m_Mutex);
    auto it = m_Records.find(entity);
    if (it == m_Records.end())
        return;

    it->second.violations = 0;
    it->second.tolerance  = m_Config.toleranceBudget;
}

// ─── Evaluation ───────────────────────────────────────────────────────────────

MovementVerdict MovementValidator::Evaluate(EntityId       entity,
                                             const Vector3& proposedPosition,
                                             std::uint64_t  serverTick,
                                             float          dtSeconds)
{
    MovementVerdict verdict;
    verdict.correctedPosition = proposedPosition;

    if (dtSeconds <= 0.0f)
    {
        verdict.decision = MovementDecision::RejectStale;
        return verdict;
    }

    std::lock_guard<std::mutex> lock(m_Mutex);

    auto it = m_Records.find(entity);
    if (it == m_Records.end())
    {
        EntityRecord fresh;
        fresh.lastPosition     = proposedPosition;
        fresh.tolerance        = m_Config.toleranceBudget;
        fresh.lastTick         = serverTick;
        fresh.lastUpdateUnixMs = NowUnixMs();
        m_Records.emplace(entity, fresh);

        verdict.toleranceRemaining = m_Config.toleranceBudget;
        return verdict;
    }

    EntityRecord& rec = it->second;

    if (serverTick < rec.lastTick)
    {
        verdict.decision        = MovementDecision::RejectStale;
        verdict.violationsCount = ++rec.violations;
        return verdict;
    }

    const float horizDistance = HorizontalDistance(proposedPosition, rec.lastPosition);
    const float verticalDelta = proposedPosition.y - rec.lastPosition.y;

    const float horizSpeed    = horizDistance / dtSeconds;
    const float vertSpeed     = std::fabs(verticalDelta) / dtSeconds;
    const float accelObserved = std::fabs(horizSpeed - rec.lastHorizontalSpeed) / dtSeconds;

    verdict.observedHorizontalSpeed = horizSpeed;
    verdict.observedVerticalSpeed   = vertSpeed;
    verdict.observedAcceleration    = accelObserved;

    rec.tolerance = std::min(
        m_Config.toleranceBudget,
        rec.tolerance + m_Config.toleranceRegenPerSec * dtSeconds);

    const float totalDelta = horizDistance + std::fabs(verticalDelta);
    if (totalDelta > m_Config.teleportDistance)
    {
        verdict.decision           = MovementDecision::RejectTeleport;
        verdict.correctedPosition  = rec.lastPosition;
        verdict.violationsCount    = ++rec.violations;
        verdict.toleranceRemaining = rec.tolerance;

        LOG_WARN(kTag,
            "Entity %u: teleport rejected — delta %.2fm (limit %.2fm)",
            static_cast<unsigned>(entity),
            totalDelta,
            m_Config.teleportDistance);
        return verdict;
    }

    if (horizSpeed > m_Config.maxHorizontalSpeed)
    {
        if (rec.tolerance >= 1.0f)
        {
            rec.tolerance -= 1.0f;
            verdict.decision = MovementDecision::AcceptSoft;
        }
        else
        {
            verdict.decision = MovementDecision::Clamp;
            verdict.correctedPosition = ClampTowards(
                rec.lastPosition, proposedPosition,
                m_Config.maxHorizontalSpeed * dtSeconds,
                m_Config.maxVerticalSpeed   * dtSeconds);
            verdict.violationsCount = ++rec.violations;
        }
    }
    else if (vertSpeed > m_Config.maxVerticalSpeed)
    {
        if (rec.tolerance >= 1.0f)
        {
            rec.tolerance -= 1.0f;
            verdict.decision = MovementDecision::AcceptSoft;
        }
        else
        {
            verdict.decision = MovementDecision::Clamp;
            verdict.correctedPosition = ClampTowards(
                rec.lastPosition, proposedPosition,
                m_Config.maxHorizontalSpeed * dtSeconds,
                m_Config.maxVerticalSpeed   * dtSeconds);
            verdict.violationsCount = ++rec.violations;
        }
    }
    else if (accelObserved > m_Config.maxAcceleration)
    {
        if (rec.tolerance >= 0.5f)
        {
            rec.tolerance -= 0.5f;
            verdict.decision = MovementDecision::AcceptSoft;
        }
        else
        {
            verdict.decision        = MovementDecision::RejectAccel;
            verdict.correctedPosition = rec.lastPosition;
            verdict.violationsCount = ++rec.violations;
        }
    }

    if (verdict.decision == MovementDecision::Accept ||
        verdict.decision == MovementDecision::AcceptSoft)
    {
        rec.lastPosition        = verdict.correctedPosition;
        rec.lastHorizontalSpeed = horizSpeed;
        rec.lastVerticalSpeed   = vertSpeed;
        rec.lastTick            = serverTick;
        rec.lastUpdateUnixMs    = NowUnixMs();
    }
    else if (verdict.decision == MovementDecision::Clamp)
    {
        rec.lastPosition        = verdict.correctedPosition;
        rec.lastHorizontalSpeed = m_Config.maxHorizontalSpeed;
        rec.lastVerticalSpeed   = 0.0f;
        rec.lastTick            = serverTick;
        rec.lastUpdateUnixMs    = NowUnixMs();
    }

    verdict.toleranceRemaining = rec.tolerance;
    verdict.violationsCount    = rec.violations;
    return verdict;
}

// ─── Queries ──────────────────────────────────────────────────────────────────

bool MovementValidator::ShouldBan(EntityId entity) const
{
    std::lock_guard<std::mutex> lock(m_Mutex);
    auto it = m_Records.find(entity);
    if (it == m_Records.end())
        return false;
    return it->second.violations >= m_Config.banThreshold;
}

std::optional<Vector3> MovementValidator::GetLastKnownPosition(EntityId entity) const
{
    std::lock_guard<std::mutex> lock(m_Mutex);
    auto it = m_Records.find(entity);
    if (it == m_Records.end())
        return std::nullopt;
    return it->second.lastPosition;
}

std::uint32_t MovementValidator::GetViolations(EntityId entity) const
{
    std::lock_guard<std::mutex> lock(m_Mutex);
    auto it = m_Records.find(entity);
    if (it == m_Records.end())
        return 0;
    return it->second.violations;
}

std::size_t MovementValidator::GetTrackedCount() const
{
    std::lock_guard<std::mutex> lock(m_Mutex);
    return m_Records.size();
}

} // namespace SagaEngine::Server::Simulation
