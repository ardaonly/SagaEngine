/// @file InterpolationBuffer.cpp
/// @brief Client-side snapshot interpolation buffer implementation.

#include "SagaEngine/Client/Interpolation/InterpolationBuffer.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace SagaEngine::Client
{

// ─── EntityInterpolationBuffer — Push ─────────────────────────────────────────

void EntityInterpolationBuffer::Push(double serverTime, const Math::Transform& transform)
{
    SnapshotEntry& slot = m_entries[m_writeIndex];
    slot.serverTime = serverTime;
    slot.transform  = transform;
    slot.valid      = true;

    m_writeIndex = (m_writeIndex + 1) % kBufferSize;
    if (m_count < kBufferSize)
        ++m_count;
}

// ─── EntityInterpolationBuffer — Sample ───────────────────────────────────────

std::optional<Math::Transform>
EntityInterpolationBuffer::Sample(double renderTime, const InterpolationConfig& config) const
{
    const SnapshotEntry* before = nullptr;
    const SnapshotEntry* after  = nullptr;

    if (!FindBracket(renderTime, before, after))
        return std::nullopt;

    // ── Compute interpolation factor t ────────────────────────────────────────

    const double span = after->serverTime - before->serverTime;
    if (span <= 0.0)
        return before->transform;

    const double elapsed = renderTime - before->serverTime;
    const float  t       = static_cast<float>(std::clamp(elapsed / span, 0.0, 1.0));

    // ── Snap threshold — teleport instead of interpolating over huge gaps ──────

    const float dist = before->transform.position.DistanceTo(after->transform.position);
    if (dist > config.snapDistanceThreshold)
        return after->transform;

    // ── Interpolate ───────────────────────────────────────────────────────────

    if (config.useSlerp)
        return before->transform.Slerp(after->transform, t);
    else
        return before->transform.Lerp(after->transform, t);
}

// ─── EntityInterpolationBuffer — Queries ──────────────────────────────────────

std::size_t EntityInterpolationBuffer::ValidCount() const noexcept
{
    return m_count;
}

double EntityInterpolationBuffer::LatestTime() const noexcept
{
    if (m_count == 0) return 0.0;

    // The most recent write is at (m_writeIndex - 1 + kBufferSize) % kBufferSize.
    const std::size_t latest = (m_writeIndex + kBufferSize - 1) % kBufferSize;
    return m_entries[latest].serverTime;
}

double EntityInterpolationBuffer::OldestTime() const noexcept
{
    if (m_count == 0) return 0.0;

    double oldest = std::numeric_limits<double>::max();
    for (std::size_t i = 0; i < m_count; ++i)
    {
        const auto& e = m_entries[i];
        if (e.valid && e.serverTime < oldest)
            oldest = e.serverTime;
    }
    return oldest;
}

void EntityInterpolationBuffer::Clear()
{
    for (auto& e : m_entries)
        e.valid = false;
    m_writeIndex = 0;
    m_count      = 0;
}

// ─── EntityInterpolationBuffer — FindBracket ─────────────────────────────────

bool EntityInterpolationBuffer::FindBracket(double                renderTime,
                                             const SnapshotEntry*& before,
                                             const SnapshotEntry*& after) const
{
    // Collect valid entries sorted by serverTime.
    const SnapshotEntry* sorted[kBufferSize] = {};
    std::size_t sortedCount = 0;

    for (std::size_t i = 0; i < kBufferSize; ++i)
    {
        if (m_entries[i].valid)
            sorted[sortedCount++] = &m_entries[i];
    }

    if (sortedCount < 2)
        return false;

    // Sort by serverTime ascending.
    std::sort(sorted, sorted + sortedCount,
        [](const SnapshotEntry* a, const SnapshotEntry* b)
        {
            return a->serverTime < b->serverTime;
        });

    // Find the bracket: the pair where before.time <= renderTime <= after.time.
    for (std::size_t i = 0; i + 1 < sortedCount; ++i)
    {
        if (sorted[i]->serverTime <= renderTime && sorted[i + 1]->serverTime >= renderTime)
        {
            before = sorted[i];
            after  = sorted[i + 1];
            return true;
        }
    }

    // Extrapolation: if renderTime is before the earliest, use the two earliest.
    if (renderTime < sorted[0]->serverTime)
    {
        before = sorted[0];
        after  = sorted[1];
        return true;
    }

    // If renderTime is past the latest, use the two most recent.
    before = sorted[sortedCount - 2];
    after  = sorted[sortedCount - 1];
    return true;
}

// ─── InterpolationManager ─────────────────────────────────────────────────────

InterpolationManager::InterpolationManager(InterpolationConfig config)
    : m_config(config)
{
}

void InterpolationManager::PushSnapshot(EntityId              entityId,
                                         double                serverTime,
                                         const Math::Transform& transform)
{
    m_buffers[entityId].Push(serverTime, transform);
}

void InterpolationManager::RemoveEntity(EntityId entityId)
{
    m_buffers.erase(entityId);
}

void InterpolationManager::Clear()
{
    m_buffers.clear();
}

std::optional<Math::Transform>
InterpolationManager::Sample(EntityId entityId, double renderTime) const
{
    const auto it = m_buffers.find(entityId);
    if (it == m_buffers.end())
        return std::nullopt;

    const double adjustedTime = renderTime - m_config.renderDelaySeconds;
    return it->second.Sample(adjustedTime, m_config);
}

std::unordered_map<EntityId, Math::Transform>
InterpolationManager::SampleAll(double renderTime) const
{
    std::unordered_map<EntityId, Math::Transform> results;
    results.reserve(m_buffers.size());

    const double adjustedTime = renderTime - m_config.renderDelaySeconds;

    for (const auto& [entityId, buffer] : m_buffers)
    {
        auto sampled = buffer.Sample(adjustedTime, m_config);
        if (sampled.has_value())
            results.emplace(entityId, sampled.value());
    }

    return results;
}

bool InterpolationManager::HasEntity(EntityId entityId) const
{
    return m_buffers.count(entityId) > 0;
}

std::size_t InterpolationManager::EntityCount() const noexcept
{
    return m_buffers.size();
}

} // namespace SagaEngine::Client
