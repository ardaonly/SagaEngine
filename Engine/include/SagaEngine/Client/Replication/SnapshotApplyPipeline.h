/// @file SnapshotApplyPipeline.h
/// @brief Client-side pipeline that folds server snapshots into the local ECS.
///
/// Layer  : SagaEngine / Client / Replication
/// Purpose: The server ships two different kinds of world data to
///          each client: full `WorldSnapshot`s (periodic or on
///          demand) and incremental `DeltaSnapshot`s (most frames).
///          On the client side both streams have to land in the same
///          `WorldState` ECS in the same order, with the same
///          consistency guarantees, so the prediction and
///          interpolation layers see a coherent authoritative
///          baseline.
///
///          The snapshot apply pipeline is the glue that sits
///          between the packet decoders and the ECS.  Packets come
///          in out of order, may reference entities the client has
///          not seen yet, and may spread across many fragments; the
///          pipeline sorts them by server tick, validates that
///          sequential deltas chain correctly, and hands a fully
///          resolved "the world is now exactly this" payload to a
///          single ECS writer.
///
/// Design rules:
///   - The pipeline is owned by the client and runs on the client's
///     render-adjacent thread, not by the server.  Nothing in this
///     header calls into server code.
///   - A delta is only applied on top of a matching baseline tick.
///     If the client missed a delta in between, the pipeline either
///     waits for the retransmit or asks for a fresh `WorldSnapshot`
///     — it never applies a delta to the wrong baseline, because
///     the resulting state would look correct but be silently
///     wrong.
///   - The pipeline owns one ECS writer instance and holds it for
///     the apply call only.  Between applies the ECS is free for
///     the render and prediction systems to read; the apply call is
///     the one moment where the ECS is briefly write-locked.
///   - Out-of-order arrivals are absorbed by a small jitter buffer
///     keyed on server tick.  The buffer has a bounded size and a
///     bounded wait; anything beyond the bounds is dropped with a
///     counter bump (operators watch the counter for tuning).
///
/// What this header is NOT:
///   - Not a packet decoder.  The decoder lives upstream and hands
///     decoded structures to this pipeline.
///   - Not an interpolator.  Interpolation sits on top of this
///     pipeline and consumes the authoritative baseline it
///     publishes.

#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

namespace SagaEngine::Simulation {
class WorldState;
} // namespace SagaEngine::Simulation

namespace SagaEngine::Client::Replication {

// ─── Tick identifiers ──────────────────────────────────────────────────────

/// Server simulation tick, stamped by the authoritative server and
/// carried through every snapshot and delta.  Monotonic during a
/// session; wraps rarely enough (at 60 Hz) that a 64-bit counter is
/// sufficient forever.
using ServerTick = std::uint64_t;
inline constexpr ServerTick kInvalidTick = 0;

// ─── Payload shapes ────────────────────────────────────────────────────────

/// Full snapshot handed to the pipeline.  The caller has already
/// decoded the wire format; this structure is the in-memory form
/// the pipeline can copy straight into the ECS.  Fields are plain
/// bytes because the pipeline is generic across component types —
/// the actual per-component logic lives inside the `Apply` callback
/// registered at setup time.
struct DecodedWorldSnapshot
{
    ServerTick                serverTick   = kInvalidTick;
    std::uint32_t             entityCount  = 0;
    std::vector<std::uint8_t> payload;              ///< Serialised ECS blob.
    std::uint32_t             crc32        = 0;     ///< Integrity check.
};

/// Incremental delta.  `baselineTick` is the tick the delta was
/// computed against on the server; the pipeline refuses to apply
/// unless its own last-applied tick matches.
struct DecodedDeltaSnapshot
{
    ServerTick                baselineTick = kInvalidTick;
    ServerTick                serverTick   = kInvalidTick;
    std::uint32_t             dirtyEntityCount = 0;
    std::vector<std::uint8_t> payload;              ///< Serialised delta blob.
};

// ─── Outcome codes ─────────────────────────────────────────────────────────

/// Result of a single apply call.  The pipeline reports these up so
/// the client network layer can react (retransmit request, stat
/// counter bump, fallback to request a new full snapshot).
enum class ApplyOutcome : std::uint8_t
{
    Ok = 0,                ///< Applied cleanly.
    BufferedForOrdering,   ///< Held in the jitter buffer; not yet applied.
    DroppedOld,            ///< Older than the current baseline; ignored.
    DroppedDuplicate,      ///< Same tick already applied.
    MissingBaseline,       ///< Delta arrived but baseline is unknown — request full snapshot.
    CrcMismatch,           ///< Snapshot failed integrity check.
    JitterOverflow,        ///< Jitter buffer full; buffered item evicted.
    InternalError,
};

// ─── Callbacks ─────────────────────────────────────────────────────────────

/// Per-snapshot apply callback.  Runs with the ECS locked for write;
/// implementations decode `payload` into component writes.  Returning
/// false signals an internal decode failure and rolls the pipeline
/// back to the previous baseline so the client can request a fresh
/// snapshot.
using FullSnapshotApplyFn = std::function<bool(
    SagaEngine::Simulation::WorldState& world,
    const DecodedWorldSnapshot&  snapshot)>;

/// Per-delta apply callback.  Same shape as above.  The pipeline
/// guarantees baseline consistency before calling: the caller can
/// assume `world` is at exactly `delta.baselineTick`.
using DeltaSnapshotApplyFn = std::function<bool(
    SagaEngine::Simulation::WorldState& world,
    const DecodedDeltaSnapshot&  delta)>;

// ─── Statistics ────────────────────────────────────────────────────────────

/// Operator-facing counters the pipeline maintains.  Cheap to read;
/// polled by the client diagnostics overlay to surface "how often
/// are we getting delta drops" at a glance.
struct SnapshotPipelineStats
{
    std::uint64_t fullApplied      = 0;
    std::uint64_t deltaApplied     = 0;
    std::uint64_t bufferedDeltas   = 0;
    std::uint64_t droppedOld       = 0;
    std::uint64_t droppedDuplicate = 0;
    std::uint64_t missingBaseline  = 0;
    std::uint64_t crcFailures      = 0;
    std::uint64_t jitterOverflow   = 0;
    ServerTick    lastAppliedTick  = kInvalidTick;
};

// ─── Configuration ─────────────────────────────────────────────────────────

/// Per-pipeline tunables.  All defaults are picked to match a 60 Hz
/// server with typical internet jitter; tight LAN tests can shrink
/// the jitter buffer without regrets.
struct SnapshotPipelineConfig
{
    /// Maximum number of out-of-order deltas held waiting for their
    /// baseline.  Larger absorbs more jitter at the cost of more
    /// memory and a longer worst-case latency.
    std::uint32_t jitterBufferSlots = 16;

    /// Hard ceiling on how old a buffered delta may get before it is
    /// evicted.  Prevents a pathological packet loss scenario from
    /// holding on to a stale delta indefinitely.
    std::uint32_t maxBufferedAgeTicks = 60;

    /// When true, a missing baseline triggers an immediate callback
    /// asking the caller to request a fresh `WorldSnapshot`.  False
    /// leaves the recovery decision to the caller, who may have a
    /// smarter retry policy.
    bool autoRequestFullSnapshotOnMiss = true;
};

/// Callback invoked when the pipeline decides it needs a full
/// snapshot to recover.  The caller typically sends a request packet
/// back to the server.
using RequestFullSnapshotFn = std::function<void()>;

// ─── Pipeline ──────────────────────────────────────────────────────────────

/// Client-side snapshot apply pipeline.  One instance per connected
/// client session.  Thread-affine — all methods must be called from
/// the same thread (typically the client's render-adjacent simulation
/// thread).
class SnapshotApplyPipeline
{
public:
    SnapshotApplyPipeline() = default;
    ~SnapshotApplyPipeline() = default;

    SnapshotApplyPipeline(const SnapshotApplyPipeline&)            = delete;
    SnapshotApplyPipeline& operator=(const SnapshotApplyPipeline&) = delete;

    /// Bind the pipeline to a live `WorldState` and install apply
    /// callbacks.  Must be called before any `SubmitFull` /
    /// `SubmitDelta`.  Reconfiguring mid-session is allowed but the
    /// pipeline resets its baseline as a safety precaution.
    bool Configure(
        SagaEngine::Simulation::WorldState* world,
        FullSnapshotApplyFn          applyFull,
        DeltaSnapshotApplyFn         applyDelta,
        const SnapshotPipelineConfig& config = {});

    /// Install the "please request a full snapshot" callback.
    /// Optional — a nullptr suppresses auto-recovery and surfaces
    /// the decision through the return code instead.
    void SetRequestFullSnapshotCallback(RequestFullSnapshotFn cb);

    /// Submit a decoded full snapshot.  Applies synchronously if the
    /// tick is newer than the current baseline; otherwise reports a
    /// drop reason.  Always clears the jitter buffer on success,
    /// because any held delta older than the new baseline is now
    /// irrelevant.
    [[nodiscard]] ApplyOutcome SubmitFull(DecodedWorldSnapshot&& snapshot);

    /// Submit a decoded delta.  Applies immediately if the baseline
    /// matches; otherwise buffers or drops according to the
    /// configured policy.
    [[nodiscard]] ApplyOutcome SubmitDelta(DecodedDeltaSnapshot&& delta);

    /// Tick the pipeline.  Promotes any buffered deltas whose
    /// baseline has just arrived, and evicts entries that exceed
    /// `maxBufferedAgeTicks`.  Call once per client frame before
    /// rendering.
    void Tick(ServerTick currentClientBaselineHint = kInvalidTick);

    /// Clear all buffered state and forget the baseline.  Used on
    /// reconnect and after an unrecoverable decode failure.
    void Reset();

    [[nodiscard]] ServerTick LastAppliedTick() const noexcept { return lastAppliedTick_; }
    [[nodiscard]] const SnapshotPipelineStats& Stats() const noexcept { return stats_; }

private:
    SagaEngine::Simulation::WorldState* world_ = nullptr;
    FullSnapshotApplyFn          applyFull_;
    DeltaSnapshotApplyFn         applyDelta_;
    RequestFullSnapshotFn        requestFullCb_;
    SnapshotPipelineConfig       config_{};

    /// Most recent server tick whose contents are fully reflected in
    /// the ECS.  The pipeline refuses any delta whose baseline does
    /// not match this value.
    ServerTick lastAppliedTick_ = kInvalidTick;

    /// Bounded ring of buffered deltas waiting for their baseline.
    /// Implementation detail — we use a small `std::vector` because
    /// the buffer is tiny (dozens of entries) and linear scan beats
    /// a hash map at that size.
    std::vector<DecodedDeltaSnapshot> jitterBuffer_;

    SnapshotPipelineStats stats_{};

    // ─── Internal helpers ──────────────────────────────────────────────────

    [[nodiscard]] ApplyOutcome BufferDelta(DecodedDeltaSnapshot&& delta);
    void PromoteBufferedDeltas();
    void EvictStaleDeltas();
};

} // namespace SagaEngine::Client::Replication
