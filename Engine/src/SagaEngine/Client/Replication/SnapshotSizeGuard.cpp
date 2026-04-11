/// @file SnapshotSizeGuard.cpp
/// @brief Snapshot size enforcement for the replication pipeline.

#include "SagaEngine/Client/Replication/SnapshotSizeGuard.h"

#include <algorithm>

namespace SagaEngine::Client::Replication {

namespace {

constexpr float kCompressionRatioAlpha = 0.05f;

} // namespace

// ─── Configure ──────────────────────────────────────────────────────────────

void SnapshotSizeGuard::Configure(SnapshotSizeConfig config) noexcept
{
    config_ = config;
}

// ─── Check full snapshot ────────────────────────────────────────────────────

SizeCheckResult SnapshotSizeGuard::CheckFullSnapshot(
    std::uint32_t payloadBytes,
    std::uint32_t entityCount) const noexcept
{
    if (payloadBytes > config_.maxFullSnapshotBytes)
        return SizeCheckResult::ExceedsMaxSize;

    if (entityCount > config_.maxEntityCount)
        return SizeCheckResult::EntityCountExceeded;

    if (payloadBytes > 0 && hasRatioSample_)
    {
        float ratio = static_cast<float>(entityCount) / static_cast<float>(payloadBytes);
        if (ratio < config_.minCompressionRatio)
            return SizeCheckResult::SuspiciousCompressionRatio;
    }

    return SizeCheckResult::Ok;
}

// ─── Check delta snapshot ───────────────────────────────────────────────────

SizeCheckResult SnapshotSizeGuard::CheckDeltaSnapshot(
    std::uint32_t payloadBytes,
    std::uint32_t entityCount) const noexcept
{
    if (payloadBytes > config_.maxDeltaSnapshotBytes)
        return SizeCheckResult::ExceedsMaxSize;

    if (entityCount > config_.maxEntityCount)
        return SizeCheckResult::EntityCountExceeded;

    return SizeCheckResult::Ok;
}

// ─── Record decoded size ────────────────────────────────────────────────────

void SnapshotSizeGuard::RecordDecodedSize(std::uint32_t payloadBytes,
                                           std::uint32_t decodedBytes) noexcept
{
    if (payloadBytes == 0)
        return;

    float ratio = static_cast<float>(decodedBytes) / static_cast<float>(payloadBytes);

    if (!hasRatioSample_)
    {
        avgCompressionRatio_ = ratio;
        hasRatioSample_ = true;
    }
    else
    {
        avgCompressionRatio_ = kCompressionRatioAlpha * ratio
                             + (1.0f - kCompressionRatioAlpha) * avgCompressionRatio_;
    }
}

float SnapshotSizeGuard::AverageCompressionRatio() const noexcept
{
    return avgCompressionRatio_;
}

} // namespace SagaEngine::Client::Replication
