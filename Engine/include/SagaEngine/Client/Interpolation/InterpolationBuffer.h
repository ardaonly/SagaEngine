/// @file InterpolationBuffer.h
/// @brief Per-entity snapshot interpolation buffer for smooth client-side rendering.
///
/// Layer  : Engine / Client / Interpolation
/// Purpose: The client receives authoritative position snapshots at the server's
///          tick rate (e.g. 20 Hz).  The render loop runs at a much higher frame
///          rate.  InterpolationBuffer stores the most recent N snapshots per
///          entity and provides a smooth, time-based interpolated transform
///          between the two surrounding snapshots.
///
/// Design:
///   - Fixed-size circular buffer per entity (default 4 entries).
///   - Push() called on each incoming DeltaSnapshot apply.
///   - Sample() called every render frame with the current interpolation time.
///   - Interpolation time is intentionally kept slightly behind real-time
///     ("render delay") so that there is always a pair of snapshots to lerp
///     between.  Typical render delay = 2 * server tick interval.
///
/// Threading: NOT thread-safe.  Expected to be called from the main/render thread.

#pragma once

#include "SagaEngine/Math/Transform.h"
#include "SagaEngine/ECS/Entity.h"

#include <array>
#include <cstdint>
#include <optional>
#include <unordered_map>

namespace SagaEngine::Client
{

using EntityId = ECS::EntityId;

// ─── Snapshot Entry ───────────────────────────────────────────────────────────

/// A single timestamped transform snapshot received from the server.
struct SnapshotEntry
{
    double          serverTime{0.0};     ///< Server-side timestamp (seconds since epoch).
    Math::Transform transform;           ///< Position / rotation / scale at that time.
    bool            valid{false};        ///< True if this slot has been written.
};

// ─── Interpolation Config ─────────────────────────────────────────────────────

/// Per-buffer tuning parameters.
struct InterpolationConfig
{
    double  renderDelaySeconds{0.1};  ///< How far behind real-time the render view trails.
    float   snapDistanceThreshold{10.0f}; ///< If gap > this, teleport instead of lerp.
    bool    useSlerp{false};          ///< Use Slerp for rotation (higher quality, slower).
};

// ─── Per-Entity Buffer ────────────────────────────────────────────────────────

/// Circular buffer of the last N snapshots for a single entity.
/// N is fixed at compile time to avoid per-entity heap allocations.
class EntityInterpolationBuffer final
{
public:
    static constexpr std::size_t kBufferSize = 4; ///< Slots in the ring buffer.

    EntityInterpolationBuffer() = default;

    // ── Push ──────────────────────────────────────────────────────────────────

    /// Store a new snapshot.  Older entries are overwritten cyclically.
    void Push(double serverTime, const Math::Transform& transform);

    // ── Sample ────────────────────────────────────────────────────────────────

    /// Compute an interpolated transform at `renderTime`.
    /// Returns std::nullopt if fewer than 2 valid snapshots exist.
    [[nodiscard]] std::optional<Math::Transform>
        Sample(double renderTime, const InterpolationConfig& config) const;

    // ── Queries ───────────────────────────────────────────────────────────────

    /// Number of valid snapshots currently in the buffer.
    [[nodiscard]] std::size_t ValidCount() const noexcept;

    /// Server time of the most recently pushed snapshot.
    [[nodiscard]] double LatestTime() const noexcept;

    /// Server time of the oldest valid snapshot in the buffer.
    [[nodiscard]] double OldestTime() const noexcept;

    /// Discard all snapshots.
    void Clear();

private:
    /// Find the two entries bracketing `renderTime` (before, after).
    /// Returns false if no valid bracket exists.
    [[nodiscard]] bool FindBracket(double           renderTime,
                                   const SnapshotEntry*& before,
                                   const SnapshotEntry*& after) const;

    std::array<SnapshotEntry, kBufferSize> m_entries{};
    std::size_t m_writeIndex{0};  ///< Next slot to write into.
    std::size_t m_count{0};       ///< Total pushes (saturates at kBufferSize).
};

// ─── Interpolation Manager ────────────────────────────────────────────────────

/// Top-level manager owning per-entity interpolation buffers.
/// The client code calls PushSnapshot() when a replication update arrives,
/// and SampleAll() every render frame to get smooth positions.
class InterpolationManager final
{
public:
    explicit InterpolationManager(InterpolationConfig config = {});
    ~InterpolationManager() = default;

    InterpolationManager(const InterpolationManager&)            = delete;
    InterpolationManager& operator=(const InterpolationManager&) = delete;

    // ── Config ────────────────────────────────────────────────────────────────

    void SetConfig(const InterpolationConfig& config) noexcept { m_config = config; }
    [[nodiscard]] const InterpolationConfig& GetConfig() const noexcept { return m_config; }

    // ── Snapshot ingestion ────────────────────────────────────────────────────

    /// Push a server snapshot for one entity.
    void PushSnapshot(EntityId entityId, double serverTime, const Math::Transform& transform);

    /// Remove all snapshot history for an entity (entity left interest).
    void RemoveEntity(EntityId entityId);

    /// Remove all entities and their histories.
    void Clear();

    // ── Sampling ──────────────────────────────────────────────────────────────

    /// Sample the interpolated transform for a single entity.
    /// Returns std::nullopt if the entity has no valid bracket.
    [[nodiscard]] std::optional<Math::Transform>
        Sample(EntityId entityId, double renderTime) const;

    /// Sample all tracked entities.  Entities without a valid bracket are omitted.
    [[nodiscard]] std::unordered_map<EntityId, Math::Transform>
        SampleAll(double renderTime) const;

    // ── Queries ───────────────────────────────────────────────────────────────

    [[nodiscard]] bool HasEntity(EntityId entityId) const;
    [[nodiscard]] std::size_t EntityCount() const noexcept;

private:
    InterpolationConfig                                        m_config;
    std::unordered_map<EntityId, EntityInterpolationBuffer>    m_buffers;
};

} // namespace SagaEngine::Client
