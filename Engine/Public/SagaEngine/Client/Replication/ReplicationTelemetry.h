/// @file ReplicationTelemetry.h
/// @brief Operational visibility for the client-side replication pipeline.
///
/// Layer  : SagaEngine / Client / Replication
/// Purpose: Exposes runtime counters, histograms, and health metrics so
///          operators can observe replication quality in production.  A
///          system that cannot be measured is blind under load.
///
/// Design rules:
///   - All counters are atomic so they can be read from any thread.
///   - The telemetry layer is append-only and lock-free; the replication
///     hot path never pays a mutex cost for observation.
///   - Counters are structured so the root cause of a desync event can
///     be classified after the fact (gap, hash mismatch, timeout, etc.).

#pragma once

#include <array>
#include <atomic>
#include <cstdint>

namespace SagaEngine::Client::Replication {

// ─── Histogram ──────────────────────────────────────────────────────────────

/// Fixed-bin histogram for latency / size distributions.
/// 32 bins, each covering a power-of-two range.
/// Bin 0: 0–1, Bin 1: 2–3, Bin 2: 4–7, … Bin 31: 2^31+.
template <std::size_t NBins = 32>
class Histogram
{
public:
    Histogram() = default;

    /// Record a value.  The value is clamped to the highest bin if it
    /// exceeds the histogram range.
    void Record(std::uint32_t value) noexcept
    {
        std::uint32_t bin = value == 0 ? 0 : static_cast<std::uint32_t>(clz32(value));
        if (bin >= NBins)
            bin = NBins - 1;
        bins_[bin].fetch_add(1, std::memory_order_relaxed);
    }

    /// Return the count in a specific bin.
    [[nodiscard]] std::uint64_t Bin(std::uint32_t index) const noexcept
    {
        return index < NBins ? bins_[index].load(std::memory_order_relaxed) : 0;
    }

    /// Total number of recorded values.
    [[nodiscard]] std::uint64_t Total() const noexcept
    {
        std::uint64_t sum = 0;
        for (std::size_t i = 0; i < NBins; ++i)
            sum += bins_[i].load(std::memory_order_relaxed);
        return sum;
    }

    /// Reset all bins to zero.
    void Reset() noexcept
    {
        for (std::size_t i = 0; i < NBins; ++i)
            bins_[i].store(0, std::memory_order_relaxed);
    }

private:
    static std::uint32_t clz32(std::uint32_t v) noexcept
    {
        // Count leading zeros — approximated with a shift loop.
        if (v == 0) return 32;
        std::uint32_t n = 0;
        while ((v & (std::uint32_t{1} << 31)) == 0) { v <<= 1; ++n; }
        return n;
    }

    std::array<std::atomic<std::uint64_t>, NBins> bins_{};
};

// ─── Desync reason codes ───────────────────────────────────────────────────

enum class DesyncReason : std::uint8_t
{
    SequenceGap,
    WorldHashMismatch,
    StalePatchFlood,
    DecodeFailure,
    ApplyFailure,
    Timeout,
    Unknown,
};

// ─── Telemetry collector ───────────────────────────────────────────────────

/// Replication telemetry snapshot.
struct ReplicationTelemetry
{
    // Apply timing (microseconds).
    std::uint32_t applyDurationP50Us = 0;
    std::uint32_t applyDurationP99Us = 0;
    std::uint32_t applyDurationMaxUs = 0;

    // Payload sizes (bytes).
    std::uint32_t avgDeltaBytes = 0;
    std::uint32_t maxDeltaBytes   = 0;
    std::uint32_t fullSnapshotBytes = 0;

    // Event counters (cumulative since start).
    std::uint64_t totalDeltasApplied   = 0;
    std::uint64_t totalFullSnapshots   = 0;
    std::uint64_t totalResyncs         = 0;
    std::uint64_t totalDesyncs         = 0;
    std::uint64_t totalGapEvents       = 0;
    std::uint64_t totalDecodeFailures  = 0;
    std::uint64_t totalApplyFailures   = 0;
    std::uint64_t totalPacketsDropped  = 0;

    // Desync classification.
    std::uint64_t desyncBySequenceGap   = 0;
    std::uint64_t desyncByHashMismatch  = 0;
    std::uint64_t desyncByStalePatches  = 0;
    std::uint64_t desyncByDecodeFailure = 0;
    std::uint64_t desyncByApplyFailure  = 0;

    // Current RTT estimate (milliseconds).
    float currentRttMs = 0.0f;

    // Current replication state index (opaque).
    std::uint8_t currentStateIndex = 0;
};

/// Global telemetry collector for the client replication pipeline.
///
/// Thread-safe: all members are atomic.  The replication hot path
/// increments counters with relaxed ordering so there is zero
/// contention.  `Snapshot()` provides a consistent read.
class ReplicationTelemetryCollector
{
public:
    static ReplicationTelemetryCollector& Instance() noexcept;

    ReplicationTelemetryCollector(const ReplicationTelemetryCollector&) = delete;
    ReplicationTelemetryCollector& operator=(const ReplicationTelemetryCollector&) = delete;

    // ─── Apply timing ─────────────────────────────────────────────────────

    /// Record the duration of a single apply operation (microseconds).
    void RecordApplyDurationUs(std::uint32_t microseconds) noexcept;

    // ─── Payload sizes ────────────────────────────────────────────────────

    /// Record the size of a decoded delta snapshot (bytes).
    void RecordDeltaBytes(std::uint32_t bytes) noexcept;

    /// Record the size of a decoded full snapshot (bytes).
    void RecordFullSnapshotBytes(std::uint32_t bytes) noexcept;

    // ─── Event counters ───────────────────────────────────────────────────

    void OnDeltaApplied() noexcept    { totalDeltasApplied_.fetch_add(1, std::memory_order_relaxed); }
    void OnFullSnapshot() noexcept    { totalFullSnapshots_.fetch_add(1, std::memory_order_relaxed); }
    void OnResync() noexcept          { totalResyncs_.fetch_add(1, std::memory_order_relaxed); }
    void OnDesync(DesyncReason reason) noexcept;
    void OnGapEvent() noexcept        { totalGapEvents_.fetch_add(1, std::memory_order_relaxed); }
    void OnDecodeFailure() noexcept   { totalDecodeFailures_.fetch_add(1, std::memory_order_relaxed); }
    void OnApplyFailure() noexcept    { totalApplyFailures_.fetch_add(1, std::memory_order_relaxed); }
    void OnPacketDropped() noexcept   { totalPacketsDropped_.fetch_add(1, std::memory_order_relaxed); }

    // ─── RTT ──────────────────────────────────────────────────────────────

    void UpdateRtt(float rttMs) noexcept;

    // ─── State ────────────────────────────────────────────────────────────

    void UpdateState(std::uint8_t stateIndex) noexcept;

    // ─── Snapshot ─────────────────────────────────────────────────────────

    /// Take a consistent snapshot of all current counters.
    [[nodiscard]] ReplicationTelemetry Snapshot() const noexcept;

    /// Reset all counters to zero (for test teardown or manual reset).
    void Reset() noexcept;

private:
    ReplicationTelemetryCollector() = default;

    Histogram<32> applyDurationHist_;
    Histogram<32> deltaSizeHist_;

    std::atomic<std::uint64_t> totalDeltasApplied_   = {0};
    std::atomic<std::uint64_t> totalFullSnapshots_   = {0};
    std::atomic<std::uint64_t> totalResyncs_         = {0};
    std::atomic<std::uint64_t> totalDesyncs_         = {0};
    std::atomic<std::uint64_t> totalGapEvents_       = {0};
    std::atomic<std::uint64_t> totalDecodeFailures_  = {0};
    std::atomic<std::uint64_t> totalApplyFailures_   = {0};
    std::atomic<std::uint64_t> totalPacketsDropped_  = {0};

    std::atomic<std::uint64_t> desyncBySequenceGap_   = {0};
    std::atomic<std::uint64_t> desyncByHashMismatch_  = {0};
    std::atomic<std::uint64_t> desyncByStalePatches_  = {0};
    std::atomic<std::uint64_t> desyncByDecodeFailure_ = {0};
    std::atomic<std::uint64_t> desyncByApplyFailure_  = {0};

    std::atomic<float>       currentRttMs_      = {0.0f};
    std::atomic<std::uint8_t> currentStateIndex_ = {0};
};

} // namespace SagaEngine::Client::Replication
