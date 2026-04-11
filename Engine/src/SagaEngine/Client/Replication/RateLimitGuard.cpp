/// @file RateLimitGuard.cpp
/// @file RateLimitGuard.cpp
/// @brief Adversarial load protection for the replication pipeline.

#include "SagaEngine/Client/Replication/RateLimitGuard.h"

#include "SagaEngine/Core/Log/Log.h"

#include <algorithm>

namespace SagaEngine::Client::Replication {

namespace {

constexpr const char* kLogTag = "Replication";

} // namespace

// ─── Configure ──────────────────────────────────────────────────────────────

void RateLimitGuard::Configure(RateLimitConfig config) noexcept
{
    config_ = config;
    tokens_.store(config_.tokenBucketCapacity, std::memory_order_relaxed);
}

// ─── Accept packet ──────────────────────────────────────────────────────────

RateLimitVerdict RateLimitGuard::AcceptPacket() noexcept
{
    // Check quarantine.
    if (quarantined_.load(std::memory_order_relaxed))
        return RateLimitVerdict::Quarantine;

    // Check token bucket.
    std::uint32_t current = tokens_.load(std::memory_order_relaxed);
    while (current > 0)
    {
        if (tokens_.compare_exchange_weak(current, current - 1, std::memory_order_relaxed))
            return RateLimitVerdict::Accept;
    }

    // No tokens available.
    return RateLimitVerdict::Throttle;
}

// ─── Decode success/failure ─────────────────────────────────────────────────

void RateLimitGuard::RecordDecodeSuccess() noexcept
{
    consecutiveDecodeFailures_.store(0, std::memory_order_relaxed);
}

RateLimitVerdict RateLimitGuard::RecordDecodeFailure() noexcept
{
    std::uint32_t failures = consecutiveDecodeFailures_.fetch_add(1, std::memory_order_relaxed) + 1;

    if (failures >= config_.maxConsecutiveDecodeFailures &&
        !quarantined_.load(std::memory_order_relaxed))
    {
        quarantined_.store(true, std::memory_order_relaxed);
        LOG_WARN(kLogTag,
                 "RateLimitGuard: %u consecutive decode failures — quarantining source",
                 static_cast<unsigned>(failures));
        return RateLimitVerdict::Quarantine;
    }

    return RateLimitVerdict::Accept;
}

// ─── Sequence gap ───────────────────────────────────────────────────────────

RateLimitVerdict RateLimitGuard::RecordSequenceGap(std::uint64_t gap) noexcept
{
    if (gap > config_.maxSequenceGap)
    {
        ++anomalyCount_;
        LOG_WARN(kLogTag,
                 "RateLimitGuard: sequence gap %llu exceeds max %llu (anomaly #%u)",
                 static_cast<unsigned long long>(gap),
                 static_cast<unsigned long long>(config_.maxSequenceGap),
                 static_cast<unsigned>(anomalyCount_));
        return RateLimitVerdict::AnomalyAlert;
    }

    return RateLimitVerdict::Accept;
}

// ─── Tick ───────────────────────────────────────────────────────────────────

void RateLimitGuard::Tick(std::uint64_t currentTick, float dtSeconds) noexcept
{
    // Refill token bucket.
    std::uint32_t refill = static_cast<std::uint32_t>(
        static_cast<float>(config_.tokenRefillRate) * dtSeconds);
    if (refill > 0)
    {
        std::uint32_t current = tokens_.load(std::memory_order_relaxed);
        std::uint32_t newTokens = std::min(current + refill, config_.tokenBucketCapacity);
        tokens_.store(newTokens, std::memory_order_relaxed);
    }

    // Check quarantine expiry.
    if (quarantined_.load(std::memory_order_relaxed) &&
        currentTick >= quarantineUntilTick_)
    {
        quarantined_.store(false, std::memory_order_relaxed);
        consecutiveDecodeFailures_.store(0, std::memory_order_relaxed);
        LOG_INFO(kLogTag, "RateLimitGuard: quarantine expired — resuming normal operation");
    }
}

std::uint32_t RateLimitGuard::AvailableTokens() const noexcept
{
    return tokens_.load(std::memory_order_relaxed);
}

bool RateLimitGuard::IsQuarantined() const noexcept
{
    return quarantined_.load(std::memory_order_relaxed);
}

} // namespace SagaEngine::Client::Replication
