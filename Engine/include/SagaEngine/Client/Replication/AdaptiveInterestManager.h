/// @file AdaptiveInterestManager.h
/// @brief Dynamic, bandwidth-aware interest management for MMO-scale replication.
///
/// Layer  : SagaEngine / Client / Replication
/// Purpose: Extends the static spatial grid with dynamic density adaptation,
///          bandwidth-budget-based relevancy degradation, and priority tiers
///          so the client gracefully reduces update load under pressure
///          instead of collapsing when the network budget is exceeded.
///
/// Design rules:
///   - Three priority tiers: Critical (always sent), Normal (distance-based
///     frequency reduction), Cosmetic (dropped first under bandwidth pressure).
///   - Dynamic cell subdivision: high-density cells split into sub-cells
///     so the interest radius adapts to local entity density.
///   - Bandwidth budget feedback loop: when the measured bytes-per-tick
///     exceed the configured budget, the manager degrades Normal and
///     Cosmetic entities by increasing their update divisor.

#pragma once

#include "SagaEngine/Client/Replication/InterestManagement.h"

#include <cstdint>
#include <unordered_map>

namespace SagaEngine::Client::Replication {

// ─── Priority tiers ────────────────────────────────────────────────────────

enum class RelevancyTier : std::uint8_t
{
    Critical,   ///< Always replicated (player character, projectiles).
    Normal,     ///< Distance-based update frequency (NPCs, vehicles).
    Cosmetic,   ///< Dropped first under bandwidth pressure (particles, decals).
};

// ─── Adaptive config ───────────────────────────────────────────────────────

struct AdaptiveInterestConfig
{
    InterestConfig baseInterest;

    /// Maximum bytes the client can receive per tick for replication.
    /// When exceeded, Normal and Cosmetic entities are throttled.
    std::uint32_t bandwidthBudgetBytesPerTick = 64 * 1024;

    /// Number of ticks between Normal entity updates when bandwidth
    /// is under pressure.  Higher means fewer updates.
    std::uint32_t normalUpdateDivisor = 4;

    /// Number of ticks between Cosmetic entity updates under pressure.
    std::uint32_t cosmeticUpdateDivisor = 16;

    /// Entity count threshold at which a cell subdivides.
    std::uint32_t cellSplitThreshold = 128;
};

// ─── Adaptive interest manager ──────────────────────────────────────────────

/// Dynamic interest manager with bandwidth feedback and priority tiers.
class AdaptiveInterestManager
{
public:
    AdaptiveInterestManager() = default;
    ~AdaptiveInterestManager() = default;

    AdaptiveInterestManager(const AdaptiveInterestManager&)            = delete;
    AdaptiveInterestManager& operator=(const AdaptiveInterestManager&) = delete;

    /// Configure the manager.  Must be called before use.
    void Configure(AdaptiveInterestConfig config) noexcept;

    /// Update the client's position.  Triggers density recalculation
    /// and potential cell subdivision.
    void UpdateClientPosition(float x, float y, float z) noexcept;

    /// Register an entity with a priority tier.
    void RegisterEntity(std::uint32_t entityId, float x, float y, float z,
                        RelevancyTier tier) noexcept;

    /// Unregister an entity.
    void UnregisterEntity(std::uint32_t entityId) noexcept;

    /// Update an entity's position.
    void UpdateEntityPosition(std::uint32_t entityId, float x, float y, float z) noexcept;

    /// Update the measured bandwidth (bytes received last tick).
    /// Triggers throttling if the budget is exceeded.
    void UpdateBandwidthMeasurement(std::uint32_t bytesThisTick) noexcept;

    /// Return true if the entity is relevant and not throttled this tick.
    /// Call once per entity per tick before replication.
    [[nodiscard]] bool IsRelevantThisTick(std::uint32_t entityId,
                                          std::uint64_t currentTick) const noexcept;

    /// Return the relevancy tier for an entity.
    [[nodiscard]] RelevancyTier GetTier(std::uint32_t entityId) const noexcept;

    /// Return the total number of relevant entities.
    [[nodiscard]] std::size_t RelevantCount() const noexcept;

private:
    AdaptiveInterestConfig config_{};
    InterestManager baseManager_;

    /// Entity -> tier mapping.
    std::unordered_map<std::uint32_t, RelevancyTier> entityTiers_;

    /// Cell -> entity count for density tracking.
    std::unordered_map<CellCoord, std::uint32_t, CellCoordHash> cellDensity_;

    /// Measured bandwidth (EMA).
    float measuredBandwidthBps_ = 0.0f;

    /// Whether bandwidth is currently over budget.
    bool bandwidthOverBudget_ = false;
};

} // namespace SagaEngine::Client::Replication
