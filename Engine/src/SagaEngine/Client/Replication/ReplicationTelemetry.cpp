/// @file ReplicationTelemetry.cpp
/// @brief Operational visibility for the client-side replication pipeline.

#include "SagaEngine/Client/Replication/ReplicationTelemetry.h"

#include <algorithm>

namespace SagaEngine::Client::Replication {

namespace {

constexpr float kPercentileThresholdP50 = 0.50f;
constexpr float kPercentileThresholdP99 = 0.99f;

/// Approximate percentile from a histogram bin array.
[[nodiscard]] std::uint32_t PercentileFromHistogram(
    const Histogram<32>& hist,
    float                threshold) noexcept
{
    std::uint64_t total = hist.Total();
    if (total == 0)
        return 0;

    std::uint64_t target = static_cast<std::uint64_t>(total * threshold);
    std::uint64_t cumulative = 0;

    for (std::uint32_t bin = 0; bin < 32; ++bin)
    {
        cumulative += hist.Bin(bin);
        if (cumulative >= target)
            return (std::uint32_t{1} << bin);
    }

    return std::uint32_t{1} << 31;
}

/// Approximate maximum value from a histogram (highest non-empty bin).
[[nodiscard]] std::uint32_t MaxFromHistogram(const Histogram<32>& hist) noexcept
{
    for (std::int32_t bin = 31; bin >= 0; --bin)
    {
        if (hist.Bin(static_cast<std::uint32_t>(bin)) > 0)
            return std::uint32_t{1} << static_cast<std::uint32_t>(bin);
    }
    return 0;
}

} // namespace

// ─── Singleton ──────────────────────────────────────────────────────────────

ReplicationTelemetryCollector& ReplicationTelemetryCollector::Instance() noexcept
{
    static ReplicationTelemetryCollector instance;
    return instance;
}

// ─── Recording ──────────────────────────────────────────────────────────────

void ReplicationTelemetryCollector::RecordApplyDurationUs(std::uint32_t microseconds) noexcept
{
    applyDurationHist_.Record(microseconds);
}

void ReplicationTelemetryCollector::RecordDeltaBytes(std::uint32_t bytes) noexcept
{
    deltaSizeHist_.Record(bytes);
}

void ReplicationTelemetryCollector::RecordFullSnapshotBytes(std::uint32_t bytes) noexcept
{
    // Full snapshots are recorded in the delta size histogram as well
    // for a unified payload size distribution.
    deltaSizeHist_.Record(bytes);
}

void ReplicationTelemetryCollector::OnDesync(DesyncReason reason) noexcept
{
    totalDesyncs_.fetch_add(1, std::memory_order_relaxed);

    switch (reason)
    {
        case DesyncReason::SequenceGap:
            desyncBySequenceGap_.fetch_add(1, std::memory_order_relaxed);
            break;
        case DesyncReason::WorldHashMismatch:
            desyncByHashMismatch_.fetch_add(1, std::memory_order_relaxed);
            break;
        case DesyncReason::StalePatchFlood:
            desyncByStalePatches_.fetch_add(1, std::memory_order_relaxed);
            break;
        case DesyncReason::DecodeFailure:
            desyncByDecodeFailure_.fetch_add(1, std::memory_order_relaxed);
            break;
        case DesyncReason::ApplyFailure:
            desyncByApplyFailure_.fetch_add(1, std::memory_order_relaxed);
            break;
        default:
            break;
    }
}

void ReplicationTelemetryCollector::UpdateRtt(float rttMs) noexcept
{
    currentRttMs_.store(rttMs, std::memory_order_relaxed);
}

void ReplicationTelemetryCollector::UpdateState(std::uint8_t stateIndex) noexcept
{
    currentStateIndex_.store(stateIndex, std::memory_order_relaxed);
}

// ─── Snapshot ───────────────────────────────────────────────────────────────

ReplicationTelemetry ReplicationTelemetryCollector::Snapshot() const noexcept
{
    ReplicationTelemetry s;

    s.applyDurationP50Us  = PercentileFromHistogram(applyDurationHist_, kPercentileThresholdP50);
    s.applyDurationP99Us  = PercentileFromHistogram(applyDurationHist_, kPercentileThresholdP99);
    s.applyDurationMaxUs  = MaxFromHistogram(applyDurationHist_);
    s.avgDeltaBytes       = deltaSizeHist_.Total() > 0
                                ? static_cast<std::uint32_t>(deltaSizeHist_.Total() / std::max<std::uint64_t>(deltaSizeHist_.Total(), 1))
                                : 0;
    s.maxDeltaBytes       = MaxFromHistogram(deltaSizeHist_);
    s.totalDeltasApplied  = totalDeltasApplied_.load(std::memory_order_relaxed);
    s.totalFullSnapshots  = totalFullSnapshots_.load(std::memory_order_relaxed);
    s.totalResyncs        = totalResyncs_.load(std::memory_order_relaxed);
    s.totalDesyncs        = totalDesyncs_.load(std::memory_order_relaxed);
    s.totalGapEvents      = totalGapEvents_.load(std::memory_order_relaxed);
    s.totalDecodeFailures = totalDecodeFailures_.load(std::memory_order_relaxed);
    s.totalApplyFailures  = totalApplyFailures_.load(std::memory_order_relaxed);
    s.totalPacketsDropped = totalPacketsDropped_.load(std::memory_order_relaxed);
    s.desyncBySequenceGap   = desyncBySequenceGap_.load(std::memory_order_relaxed);
    s.desyncByHashMismatch  = desyncByHashMismatch_.load(std::memory_order_relaxed);
    s.desyncByStalePatches  = desyncByStalePatches_.load(std::memory_order_relaxed);
    s.desyncByDecodeFailure = desyncByDecodeFailure_.load(std::memory_order_relaxed);
    s.desyncByApplyFailure  = desyncByApplyFailure_.load(std::memory_order_relaxed);
    s.currentRttMs          = currentRttMs_.load(std::memory_order_relaxed);
    s.currentStateIndex     = currentStateIndex_.load(std::memory_order_relaxed);

    return s;
}

void ReplicationTelemetryCollector::Reset() noexcept
{
    applyDurationHist_.Reset();
    deltaSizeHist_.Reset();
    totalDeltasApplied_.store(0, std::memory_order_relaxed);
    totalFullSnapshots_.store(0, std::memory_order_relaxed);
    totalResyncs_.store(0, std::memory_order_relaxed);
    totalDesyncs_.store(0, std::memory_order_relaxed);
    totalGapEvents_.store(0, std::memory_order_relaxed);
    totalDecodeFailures_.store(0, std::memory_order_relaxed);
    totalApplyFailures_.store(0, std::memory_order_relaxed);
    totalPacketsDropped_.store(0, std::memory_order_relaxed);
    desyncBySequenceGap_.store(0, std::memory_order_relaxed);
    desyncByHashMismatch_.store(0, std::memory_order_relaxed);
    desyncByStalePatches_.store(0, std::memory_order_relaxed);
    desyncByDecodeFailure_.store(0, std::memory_order_relaxed);
    desyncByApplyFailure_.store(0, std::memory_order_relaxed);
    currentRttMs_.store(0.0f, std::memory_order_relaxed);
    currentStateIndex_.store(0, std::memory_order_relaxed);
}

} // namespace SagaEngine::Client::Replication
