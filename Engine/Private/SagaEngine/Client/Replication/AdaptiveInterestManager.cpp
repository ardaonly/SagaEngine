/// @file AdaptiveInterestManager.cpp
/// @brief Dynamic, bandwidth-aware interest management for MMO-scale replication.

#include "SagaEngine/Client/Replication/AdaptiveInterestManager.h"

#include <cmath>

namespace SagaEngine::Client::Replication {

namespace {

constexpr const char* kLogTag = "Replication";
constexpr float kBandwidthEmaAlpha = 0.1f;

} // namespace

// ─── Configure ──────────────────────────────────────────────────────────────

void AdaptiveInterestManager::Configure(AdaptiveInterestConfig config) noexcept
{
    config_ = config;
    baseManager_.Configure(config.baseInterest);
}

// ─── Client position ────────────────────────────────────────────────────────

void AdaptiveInterestManager::UpdateClientPosition(float x, float y, float z) noexcept
{
    baseManager_.UpdateClientPosition(Math::Vec3{x, y, z});
}

// ─── Entity registration ───────────────────────────────────────────────────

void AdaptiveInterestManager::RegisterEntity(std::uint32_t entityId, float x, float y, float z,
                                              RelevancyTier tier) noexcept
{
    baseManager_.RegisterEntity(entityId, Math::Vec3{x, y, z});
    entityTiers_[entityId] = tier;

    // Note: baseManager_.WorldToCell returns CellCoord but InterestManager
    // uses ECS::EntityId (uint32_t) while we use std::uint32_t.  This is
    // compatible since ECS::EntityId is a uint32_t alias.
    CellCoord cell = baseManager_.WorldToCell(Math::Vec3{x, y, z});
    cellDensity_[cell]++;
}

void AdaptiveInterestManager::UnregisterEntity(std::uint32_t entityId) noexcept
{
    auto tierIt = entityTiers_.find(entityId);
    if (tierIt != entityTiers_.end())
        entityTiers_.erase(tierIt);

    baseManager_.UnregisterEntity(static_cast<ECS::EntityId>(entityId));
}

void AdaptiveInterestManager::UpdateEntityPosition(std::uint32_t entityId,
                                                    float x, float y, float z) noexcept
{
    // Remove old cell density.
    auto tierIt = entityTiers_.find(entityId);
    if (tierIt != entityTiers_.end())
    {
        // We need the old position — in a production system the entity's
        // last position would be cached.  For now, skip density tracking.
    }

    baseManager_.UpdateEntityPosition(entityId, Math::Vec3{x, y, z});
}

// ─── Bandwidth feedback ─────────────────────────────────────────────────────

void AdaptiveInterestManager::UpdateBandwidthMeasurement(std::uint32_t bytesThisTick) noexcept
{
    // EMA smoothing.
    if (measuredBandwidthBps_ < 0.001f)
        measuredBandwidthBps_ = static_cast<float>(bytesThisTick);
    else
        measuredBandwidthBps_ = kBandwidthEmaAlpha * static_cast<float>(bytesThisTick)
                              + (1.0f - kBandwidthEmaAlpha) * measuredBandwidthBps_;

    bandwidthOverBudget_ = measuredBandwidthBps_ > static_cast<float>(config_.bandwidthBudgetBytesPerTick);
}

// ─── Relevancy check ────────────────────────────────────────────────────────

bool AdaptiveInterestManager::IsRelevantThisTick(std::uint32_t entityId,
                                                  std::uint64_t currentTick) const noexcept
{
    if (!baseManager_.IsRelevant(entityId))
        return false;

    auto tierIt = entityTiers_.find(entityId);
    if (tierIt == entityTiers_.end())
        return false;

    RelevancyTier tier = tierIt->second;

    // Critical entities are always relevant.
    if (tier == RelevancyTier::Critical)
        return true;

    // Under bandwidth pressure, throttle Normal and Cosmetic.
    if (bandwidthOverBudget_)
    {
        if (tier == RelevancyTier::Cosmetic)
            return (currentTick % config_.cosmeticUpdateDivisor) == 0;
        if (tier == RelevancyTier::Normal)
            return (currentTick % config_.normalUpdateDivisor) == 0;
    }

    return true;
}

RelevancyTier AdaptiveInterestManager::GetTier(std::uint32_t entityId) const noexcept
{
    auto it = entityTiers_.find(entityId);
    return (it != entityTiers_.end()) ? it->second : RelevancyTier::Cosmetic;
}

std::size_t AdaptiveInterestManager::RelevantCount() const noexcept
{
    return baseManager_.RelevantCount();
}

} // namespace SagaEngine::Client::Replication
