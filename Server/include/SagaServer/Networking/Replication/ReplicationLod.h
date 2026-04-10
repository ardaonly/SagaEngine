/// @file ReplicationLod.h
/// @brief Distance-based level-of-detail tiers for replication update rate.
///
/// Layer  : SagaServer / Networking / Replication
/// Purpose: Sending every entity at the full simulation tick rate wastes
///          bandwidth on distant players that cannot tell the difference.
///          This header defines a small set of LOD tiers and the rule that
///          maps "distance from viewer" to "how often do I get a snapshot?".
///
/// Integration:
///   - ReplicationGraph consults `ResolveLod()` during relevancy scoring,
///     once per (entity, client) pair.
///   - The resolved `LodLevel` becomes a divisor on the client's tick
///     cadence: `shouldSend = (currentTick % tier.updateEveryNTicks == 0)`.
///   - Priority classes (ReplicationPriority.h) override the LOD decision:
///     Critical entities bypass LOD entirely and always update at 1:1.
///
/// Design:
///   - Four tiers chosen to mirror the existing PriorityClass enum width
///     so both can be stored in the same hot data structures.
///   - Thresholds are stored as squared distances to avoid sqrt on the
///     hot replication path — callers pass the squared distance directly.
///   - Tiers are compile-time constants by default but the graph owns a
///     `ReplicationLodConfig` it can swap at runtime for stress tests /
///     bandwidth emergencies.

#pragma once

#include <cstdint>

namespace SagaEngine::Networking::Replication
{

// ─── LOD levels ──────────────────────────────────────────────────────────────

/// Four tiers of replication cadence.  The names describe what the tier is
/// *for*, not the numeric update rate, so default thresholds can be tuned
/// without call-site churn.
enum class LodLevel : std::uint8_t
{
    Full   = 0, ///< Every tick — nearby entities in immediate combat range.
    High   = 1, ///< Every other tick — mid-range entities still visible.
    Low    = 2, ///< Every 4 ticks — distant entities, coarse updates.
    Sparse = 3  ///< Every 16 ticks — far entities that barely move on screen.
};

// ─── Tier definition ─────────────────────────────────────────────────────────

/// Definition of one LOD tier: its tick divisor and its squared-distance
/// upper bound.  A tier is selected when the viewer→entity squared distance
/// is ≤ `maxDistanceSq`.
struct LodTier
{
    LodLevel     level;
    std::uint32_t updateEveryNTicks;
    float        maxDistanceSq;
};

// ─── Default configuration ───────────────────────────────────────────────────

/// Immutable defaults.  These values assume a metric world where a player's
/// "close combat" range is about 20 m.  Tune via `ReplicationLodConfig` for
/// games with different scales.
struct ReplicationLodDefaults
{
    /// Full-rate radius: 20 m → squared 400.
    static constexpr float kFullRadiusSq   =     400.0f;
    /// High-rate radius: 60 m → squared 3600.
    static constexpr float kHighRadiusSq   =    3600.0f;
    /// Low-rate radius: 200 m → squared 40000.
    static constexpr float kLowRadiusSq    =   40000.0f;
    /// Beyond the Low radius everything drops to Sparse.  No upper bound.
    static constexpr float kSparseRadiusSq = 1.0e30f;
};

// ─── Runtime configuration ───────────────────────────────────────────────────

/// Runtime-mutable LOD table.  Owned by the ReplicationGraph so an operator
/// can, for example, halve all update rates under a bandwidth emergency.
class ReplicationLodConfig
{
public:
    /// Construct with the built-in defaults.
    ReplicationLodConfig() noexcept
    {
        tiers_[0] = {LodLevel::Full,   1,  ReplicationLodDefaults::kFullRadiusSq};
        tiers_[1] = {LodLevel::High,   2,  ReplicationLodDefaults::kHighRadiusSq};
        tiers_[2] = {LodLevel::Low,    4,  ReplicationLodDefaults::kLowRadiusSq};
        tiers_[3] = {LodLevel::Sparse, 16, ReplicationLodDefaults::kSparseRadiusSq};
    }

    /// Override a single tier.  `level` must be a valid `LodLevel` and is
    /// used as the array index.
    void SetTier(LodLevel level,
                 std::uint32_t updateEveryNTicks,
                 float maxDistanceSq) noexcept
    {
        const auto i = static_cast<std::size_t>(level);
        if (i < kTierCount)
        {
            tiers_[i].updateEveryNTicks = updateEveryNTicks;
            tiers_[i].maxDistanceSq     = maxDistanceSq;
        }
    }

    /// Map a squared distance to the matching tier.
    [[nodiscard]] const LodTier& Resolve(float distanceSq) const noexcept
    {
        // Linear scan — kTierCount is 4, branch-predictable, faster than a
        // binary search and generates better code for the common "Full"
        // case which is hit by the majority of relevant entities.
        for (std::size_t i = 0; i + 1 < kTierCount; ++i)
            if (distanceSq <= tiers_[i].maxDistanceSq)
                return tiers_[i];
        return tiers_[kTierCount - 1];
    }

    /// Test whether a given entity should transmit on the given tick.
    /// `tick` is the server's current simulation tick number.
    [[nodiscard]] bool ShouldTransmit(LodLevel level,
                                      std::uint64_t tick) const noexcept
    {
        const auto i = static_cast<std::size_t>(level);
        if (i >= kTierCount)
            return true;
        const std::uint32_t n = tiers_[i].updateEveryNTicks;
        return (n <= 1) || (tick % n == 0);
    }

private:
    static constexpr std::size_t kTierCount = 4;
    LodTier tiers_[kTierCount]{};
};

} // namespace SagaEngine::Networking::Replication
