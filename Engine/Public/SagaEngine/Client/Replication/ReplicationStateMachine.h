/// @file ReplicationStateMachine.h
/// @brief State machine governing client-side replication lifecycle.
///
/// Layer  : SagaEngine / Client / Replication
/// Purpose: Tracks the client's synchronization state with the server,
///          decides which incoming packets to accept, buffer, or drop,
///          and triggers self-healing (resync) when divergence is detected.
///          The state machine is the gate between the wire decoders and
///          the snapshot apply pipeline — nothing reaches the ECS without
///          passing through here first.
///
/// Design rules:
///   - Stateless packets → stateful processing.  The state machine owns
///     the sequence window, gap detector, and RTT estimator so the
///     pipeline above can remain a pure function.
///   - Per-state acceptance table: each state declares which packet
///     types are Accepted, Buffered, or Dropped.  No ad-hoc if/else
///     chains buried in apply logic.
///   - RTT-aware gap detection: the gap threshold scales with measured
///     round-trip time so a laggy connection does not trigger false
///     desync flags.
///   - Self-healing: on Desynced state the machine auto-requests a full
///     snapshot after a configurable grace period.

#pragma once

#include "SagaEngine/Client/Replication/SnapshotApplyPipeline.h"

#include <array>
#include <cstdint>
#include <functional>

namespace SagaEngine::Client::Replication {

// ─── Replication states ────────────────────────────────────────────────────

/// Lifecycle states for the client's replication session.
enum class ReplicationState : std::uint8_t
{
    Boot,                   ///< Initial state; no data received yet.
    AwaitingFullSnapshot,   ///< Requested full snapshot; waiting for arrival.
    SyncingBaseline,        ///< Full snapshot received; applying to ECS.
    Synced,                 ///< Baseline established; accepting deltas.
    Desynced,               ///< Divergence detected; dropping packets.
    ResyncRequested,        ///< Client asked for resync; awaiting full snapshot.
};

/// Human-readable state name.
[[nodiscard]] const char* StateToString(ReplicationState state) noexcept;

// ─── Accept result ─────────────────────────────────────────────────────────

/// Outcome of the state machine's packet acceptance decision.
enum class AcceptResult : std::uint8_t
{
    Accept,     ///< Packet is valid for current state; proceed to pipeline.
    Buffer,     ///< Packet is valid but held until state advances.
    Drop,       ///< Packet rejected in current state; discard silently.
};

// ─── Sequence window ───────────────────────────────────────────────────────

/// Sliding window tracker for received sequence numbers.
///
/// Tracks which sequences have been received, detects gaps, and
/// reports when the gap count exceeds the RTT-aware threshold.
/// The window covers the most recent 64 sequences (one bit each).
class SequenceWindow
{
public:
    SequenceWindow() = default;

    /// Record a received sequence number.  Returns true if this is
    /// a new sequence (not a duplicate).
    [[nodiscard]] bool Record(std::uint64_t seq) noexcept;

    /// Return the count of missing sequences between the lowest
    /// unacked and the highest received.
    [[nodiscard]] std::uint32_t GapCount() const noexcept;

    /// Dynamic threshold: max(8, 2 + rttMs / 50).  Scales with
    /// network conditions so a laggy connection does not flag false
    /// desync on normal reordering.
    [[nodiscard]] std::uint32_t GapThreshold(float rttMs) const noexcept;

    /// Return true if the gap count exceeds the threshold for the
    /// given RTT — signals a likely desync.
    [[nodiscard]] bool IsDesynced(float rttMs) const noexcept;

    /// Reset the window to initial state.
    void Reset() noexcept;

    [[nodiscard]] std::uint64_t HighestReceived() const noexcept { return highestReceived_; }

    /// Check if a sequence has already been seen (for replay detection).
    [[nodiscard]] bool IsSeen(std::uint64_t seq) const noexcept;

private:
    std::uint64_t highestReceived_   = 0;
    std::uint64_t windowMask_        = 0;   ///< Bit 0 = oldest, bit 63 = newest.
    static constexpr std::uint32_t kWindowSize = 64;
};

// ─── RTT estimator ─────────────────────────────────────────────────────────

/// Exponential moving average RTT tracker.
///
/// Smooths out jitter so the gap threshold responds to sustained
/// latency changes rather than single-packet spikes.
class RttEstimator
{
public:
    RttEstimator() = default;

    /// Feed a new RTT sample (milliseconds).  Updates the EMA.
    void AddSample(float rttMs) noexcept;

    [[nodiscard]] float Current() const noexcept { return rttMs_; }
    void Reset() noexcept { rttMs_ = 0.0f; }

private:
    float rttMs_ = 0.0f;
    static constexpr float kAlpha = 0.1f;  ///< EMA smoothing factor.
};

// ─── State machine config ──────────────────────────────────────────────────

/// Tunables for the replication state machine.
struct StateMachineConfig
{
    /// Ticks between automatic world hash comparisons (desync check).
    /// Default 300 = 5 seconds at 60 Hz.  Zero disables.
    std::uint32_t hashCheckIntervalTicks = 300;

    /// Ticks to wait in Desynced state before auto-requesting a
    /// full snapshot.  Prevents thrashing on transient errors.
    std::uint32_t desyncGraceTicks = 2;

    /// Stale generation patch rate threshold (per second).  If the
    /// rate of generation-mismatched patches exceeds this value,
    /// the machine assumes a replay attack or severe desync and
    /// triggers a hard resync.
    std::uint32_t stalePatchRateThreshold = 32;
};

// ─── State machine stats ───────────────────────────────────────────────────

/// Operator-facing counters for replication health monitoring.
struct StateMachineStats
{
    std::uint64_t packetsAccepted  = 0;
    std::uint64_t packetsBuffered  = 0;
    std::uint64_t packetsDropped   = 0;
    std::uint64_t gapEvents        = 0;
    std::uint64_t desyncEvents     = 0;
    std::uint64_t resyncRequests   = 0;
    std::uint64_t stalePatches     = 0;
    std::uint64_t hashChecks       = 0;
    std::uint64_t hashMismatches   = 0;
    float         currentRttMs     = 0.0f;
};

// ─── State machine ─────────────────────────────────────────────────────────

/// Client-side replication state machine.
///
/// Thread-affine: all methods must be called from the game thread.
/// The state machine does not own the pipeline or the decoders — it
/// is a pure decision engine.  Call `AcceptPacket()` before feeding
/// any decoded snapshot into the pipeline.
class ReplicationStateMachine
{
public:
    ReplicationStateMachine() = default;
    ~ReplicationStateMachine() = default;

    ReplicationStateMachine(const ReplicationStateMachine&)            = delete;
    ReplicationStateMachine& operator=(const ReplicationStateMachine&) = delete;

    /// Configure the state machine.  Must be called before use.
    void Configure(StateMachineConfig config = {}) noexcept;

    /// Current replication state.
    [[nodiscard]] ReplicationState CurrentState() const noexcept { return state_; }

    /// Transition to a new state.  Logs state changes and resets
    /// per-state counters as needed.
    void TransitionTo(ReplicationState newState) noexcept;

    /// Decide whether to accept, buffer, or drop an incoming packet.
    /// Call this before passing the packet to the pipeline.
    /// Note: Does not check for replayed sequences; use IsSequenceSeen() if needed.
    [[nodiscard]] AcceptResult AcceptPacket(ServerTick tick, bool isFullSnapshot) const noexcept;

    /// Check if a sequence number has already been recorded.
    /// Used for replay detection - returns true for duplicate/old sequences.
    [[nodiscard]] bool IsSequenceSeen(std::uint64_t seq) const noexcept;

    /// Record a received sequence number and check for gaps.
    /// May trigger Desynced transition if gap threshold exceeded.
    void RecordSequence(std::uint64_t seq) noexcept;

    /// Feed a new RTT sample.  Updates the gap threshold dynamically.
    void UpdateRtt(float rttMs) noexcept;

    /// Tick the state machine once per frame.  Handles hash check
    /// cadence, desync grace period, and auto-resync triggers.
    void Tick(ServerTick currentTick) noexcept;

    /// Callback: invoked when the machine decides it needs a full
    /// snapshot to recover.  The caller typically sends a request
    /// packet back to the server.
    using RequestFullSnapshotFn = std::function<void()>;
    void SetRequestFullSnapshotCallback(RequestFullSnapshotFn cb) noexcept;

    /// Callback: invoked when a world hash mismatch is detected.
    /// The caller should compare client and server hashes.
    using OnDesyncDetectedFn = std::function<void(
        ServerTick tick,
        std::uint64_t clientHash,
        std::uint64_t serverHash)>;
    void SetOnDesyncDetectedCallback(OnDesyncDetectedFn cb) noexcept;

    /// Reset to Boot state.  Clears all buffered data and counters.
    void Reset() noexcept;

    [[nodiscard]] const StateMachineStats& Stats() const noexcept { return stats_; }

private:
    ReplicationState state_ = ReplicationState::Boot;
    StateMachineConfig config_{};
    StateMachineStats stats_{};

    SequenceWindow  sequenceWindow_;
    RttEstimator    rttEstimator_;

    ServerTick lastHashCheckTick_ = 0;
    ServerTick desyncEnteredTick_ = 0;

    /// Rate tracker for stale generation patches.
    std::uint32_t stalePatchCountThisSecond_ = 0;
    ServerTick    stalePatchWindowStart_     = 0;

    RequestFullSnapshotFn requestFullCb_;
    OnDesyncDetectedFn    onDesyncCb_;
};

} // namespace SagaEngine::Client::Replication
