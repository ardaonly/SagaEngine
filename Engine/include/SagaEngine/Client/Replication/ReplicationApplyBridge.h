/// @file ReplicationApplyBridge.h
/// @brief Glue between `SnapshotApplyPipeline`, `ReconciliationBuffer`, and
///        `InterpolationManager`.
///
/// Layer  : SagaEngine / Client / Replication
/// Purpose: The snapshot apply pipeline guarantees *ordered, consistent*
///          delivery of decoded world snapshots to the client ECS.  It does
///          *not* know what to do with the local player (prediction /
///          reconciliation) or with remote entities (snapshot
///          interpolation).  That policy lives here.
///
///          The bridge takes the per-entity state that the caller has
///          already decoded out of a `DecodedWorldSnapshot` /
///          `DecodedDeltaSnapshot` payload and:
///
///            - Routes the *local player's* authoritative state into the
///              `ReconciliationBuffer`, which rewinds and replays any
///              predicted inputs the server has not yet acked.
///            - Routes every *remote entity's* transform into the
///              `InterpolationManager`, which hands a smoothed transform
///              back to the renderer every frame.
///
///          By sitting between the decoder and the renderer the bridge
///          gives both sides a stable contract: the decoder owns the wire
///          format, the bridge owns the client policy.  Neither has to
///          know about the other.
///
/// Design rules:
///   - Single responsibility.  The bridge converts; it does not decode
///     wire bytes (that is the caller's job) and it does not touch the
///     ECS directly (that is `SnapshotApplyPipeline`'s job).
///   - Non-owning references to the pipeline, the reconciliation buffer,
///     the interpolation manager and the world.  The client session owns
///     all four; the bridge must not outlive any of them.
///   - Thread-affine.  All methods are expected to run on the client
///     simulation thread, same as `SnapshotApplyPipeline::Tick`.
///
/// What this header is NOT:
///   - Not a decoder.  `SnapshotDecodeFn` is supplied by the caller.
///   - Not a simulator.  The replay callback is supplied by the caller
///     and is the *only* path that touches player movement integration.
///   - Not an ECS writer.  Full/delta writes still run through the
///     apply callbacks that the caller installs on the pipeline;
///     the bridge simply adds the prediction / interpolation side
///     effects on top of a successful apply.

#pragma once

#include "SagaEngine/Client/Interpolation/InterpolationBuffer.h"
#include "SagaEngine/Client/Prediction/ReconciliationBuffer.h"
#include "SagaEngine/Client/Replication/SnapshotApplyPipeline.h"
#include "SagaEngine/ECS/Entity.h"
#include "SagaEngine/Math/Transform.h"
#include "SagaEngine/Math/Vec3.h"

#include <cstdint>
#include <functional>
#include <vector>

namespace SagaEngine::Simulation {
class WorldState;
} // namespace SagaEngine::Simulation

namespace SagaEngine::Client::Replication {

// ─── Decoded per-entity state ─────────────────────────────────────────────

/// Intermediate representation the caller hands to the bridge after
/// decoding a snapshot / delta payload.  Intentionally minimal: every
/// client cares about transform + velocity, so those are baked in;
/// anything richer (health, quest state) belongs in the caller's own
/// apply callback, not here.
struct SnapshotEntityState
{
    SagaEngine::ECS::EntityId entityId = 0; ///< Replicated entity id.
    SagaEngine::Math::Transform transform{}; ///< Authoritative transform at the server tick.
    SagaEngine::Math::Vec3      velocity{};  ///< Linear velocity for replay integration.
};

/// Packaged result of a decode pass.  The bridge consumes one of these
/// per `SubmitFull` / `SubmitDelta` call.
struct DecodedSnapshotFrame
{
    std::vector<SnapshotEntityState> entities;     ///< Every replicated entity in the frame.
    std::uint32_t                    ackedInputSeq = 0; ///< Last input the server processed for the local player.
    SagaEngine::ECS::EntityId        localPlayerId = 0; ///< Set if the local player appears in `entities`.
    bool                             hasLocalPlayer = false; ///< Convenience flag set by the decoder.
};

/// Decoder callback.  Turns an opaque payload (wire bytes already
/// extracted from a `DecodedWorldSnapshot` / `DecodedDeltaSnapshot`)
/// into the entity-state list the bridge routes.  Returning false
/// signals a decode failure — the bridge propagates the failure back
/// through the pipeline so the client can request a fresh full snapshot.
using SnapshotFrameDecodeFn = std::function<bool(
    const std::vector<std::uint8_t>& payload,
    DecodedSnapshotFrame&            outFrame)>;

// ─── Configuration ────────────────────────────────────────────────────────

/// Tunables for the bridge.  Defaults target a 60 Hz client talking to a
/// 20 Hz server with ~100 ms of render delay; the tests override every
/// field so the defaults only have to be sensible, not optimal.
struct ReplicationApplyBridgeConfig
{
    /// The entity id whose state should be routed into the
    /// `ReconciliationBuffer` instead of the `InterpolationManager`.
    /// Must match the id used by the client prediction loop when it
    /// records its own predictions.  A value of 0 disables the
    /// prediction path entirely (dedicated-observer clients).
    SagaEngine::ECS::EntityId localPlayerId = 0;

    /// Client fixed time step (seconds).  Forwarded verbatim to
    /// `ReconciliationBuffer::AcknowledgeAndReconcile` every time an
    /// ack lands.  Must match the step the prediction loop uses.
    float fixedDtSeconds = 1.0f / 60.0f;

    /// Squared position error threshold that triggers a rewind-and-replay
    /// on reconcile.  Default is 0.04 m² (= 20 cm).  Tighter values catch
    /// more drift at the cost of more replays; looser values let a fast
    /// player smooth out small hitches without rewinding.
    float positionErrorThresholdSq = 0.04f;

    /// Server tick rate in hertz.  Used to convert `ServerTick` into the
    /// `double` seconds-since-epoch that `InterpolationManager` wants.
    /// Must match the server's `ReplicationManager::SetReplicationFrequency`
    /// value; mismatches produce a constant jitter in the interpolation.
    double serverTickHz = 20.0;
};

// ─── Statistics ───────────────────────────────────────────────────────────

/// Observable counters — polled by the diagnostics overlay and by
/// regression tests that want to verify a specific apply path fired.
struct ReplicationApplyBridgeStats
{
    std::uint64_t fullSnapshotsRouted    = 0; ///< Successful full-snapshot decodes.
    std::uint64_t deltaSnapshotsRouted   = 0; ///< Successful delta-snapshot decodes.
    std::uint64_t fullDecodeFailures     = 0; ///< Decoder returned false on a full snapshot.
    std::uint64_t deltaDecodeFailures    = 0; ///< Decoder returned false on a delta.
    std::uint64_t localPlayerAcks        = 0; ///< Frames that carried a local player state.
    std::uint64_t remoteEntitySnapshots  = 0; ///< Total remote transforms pushed to interpolation.
    std::uint64_t reconcileRewinds       = 0; ///< Reconciles that triggered a replay (errorSq > threshold).
    std::uint32_t lastAckedInputSeq      = 0; ///< Most recent input seq the server confirmed.
};

// ─── Bridge ───────────────────────────────────────────────────────────────

/// Installs the apply callbacks on a `SnapshotApplyPipeline` so each
/// successful apply drives the prediction and interpolation layers.  The
/// caller still owns the pipeline, the reconciliation buffer, and the
/// interpolation manager; the bridge only keeps non-owning pointers.
class ReplicationApplyBridge
{
public:
    ReplicationApplyBridge() = default;
    ~ReplicationApplyBridge() = default;

    ReplicationApplyBridge(const ReplicationApplyBridge&)            = delete;
    ReplicationApplyBridge& operator=(const ReplicationApplyBridge&) = delete;
    ReplicationApplyBridge(ReplicationApplyBridge&&)                 = delete;
    ReplicationApplyBridge& operator=(ReplicationApplyBridge&&)      = delete;

    /// Bind the bridge to its dependencies and install the apply
    /// callbacks on the pipeline.  The pipeline is `Configure`d with
    /// bridge-owned callbacks that wrap the caller's decoders and
    /// route the decoded state to prediction / interpolation.  The
    /// world pointer is forwarded verbatim to
    /// `SnapshotApplyPipeline::Configure` — the caller is still free
    /// to hold its own pointer to the same world for ECS writes.
    ///
    /// The `reconciliation` and `interpolation` parameters are optional
    /// — a null pointer disables the corresponding side of the bridge.
    /// At least one must be non-null or the call returns false.
    bool Configure(
        SnapshotApplyPipeline&                                      pipeline,
        SagaEngine::Simulation::WorldState*                         world,
        SagaEngine::Client::Prediction::ReconciliationBuffer<>*     reconciliation,
        SagaEngine::Client::InterpolationManager*                   interpolation,
        SnapshotFrameDecodeFn                                       decodeFull,
        SnapshotFrameDecodeFn                                       decodeDelta,
        const ReplicationApplyBridgeConfig&                         config = {},
        const SnapshotPipelineConfig&                               pipelineConfig = {});

    /// Install the replay callback forwarded into
    /// `ReconciliationBuffer::AcknowledgeAndReconcile`.  Optional — if
    /// the caller never sets a replay function the reconcile path still
    /// runs the ack-prune step but skips the replay, which is fine for
    /// observer / replay-mode clients.
    void SetReplayFn(
        SagaEngine::Client::Prediction::ReconciliationBuffer<>::ReplayFn fn);

    /// Clear all routing state.  Called on reconnect / zone transfer
    /// to drop any stale buffered snapshots or prediction history.
    void Reset();

    // ── Introspection ─────────────────────────────────────────────────────

    [[nodiscard]] const ReplicationApplyBridgeStats& GetStats() const noexcept
    {
        return stats_;
    }

    /// Most recent reconciled state returned by the reconciliation
    /// buffer.  The client prediction loop reads this after every
    /// `SubmitFull` / `SubmitDelta` to snap its local player transform
    /// to the authoritative value.
    [[nodiscard]] const SagaEngine::Client::Prediction::PredictedState&
        LastReconciledState() const noexcept
    {
        return lastReconciledState_;
    }

    /// True if the bridge saw the local player in the most recent frame.
    /// Consumers use this to skip the reconcile step on frames that
    /// carried only remote entities.
    [[nodiscard]] bool LastFrameHadLocalPlayer() const noexcept
    {
        return lastFrameHadLocalPlayer_;
    }

private:
    // ─── Apply handlers ──────────────────────────────────────────────────

    /// Pipeline callback fired for every successful full snapshot apply.
    /// Decodes the payload, routes the local player into the
    /// reconciliation buffer, and pushes every remote entity into the
    /// interpolation manager.
    bool OnFullApply(SagaEngine::Simulation::WorldState& world,
                     const DecodedWorldSnapshot&         snapshot);

    /// Pipeline callback fired for every successful delta apply.
    /// Same routing logic as `OnFullApply`, but uses the delta decoder
    /// and the delta's `serverTick` for the interpolation time base.
    bool OnDeltaApply(SagaEngine::Simulation::WorldState&  world,
                      const DecodedDeltaSnapshot&          delta);

    // ─── Routing helpers ─────────────────────────────────────────────────

    /// Push every remote entity from `frame` into the interpolation
    /// manager with `serverTick` translated to seconds.
    void RouteRemoteEntities(const DecodedSnapshotFrame& frame,
                             ServerTick                  serverTick);

    /// Run the reconciliation step for the local player.  `ackedInputSeq`
    /// is the last input the server processed; the authoritative transform
    /// is taken from the local-player entry inside `frame`.
    void RouteLocalPlayer(const DecodedSnapshotFrame& frame);

    /// Convert a `ServerTick` into the seconds-since-epoch coordinate
    /// system that `InterpolationManager::PushSnapshot` expects.
    [[nodiscard]] double TickToSeconds(ServerTick tick) const noexcept;

    // ─── Members ─────────────────────────────────────────────────────────

    SnapshotApplyPipeline*                                 pipeline_       = nullptr;
    SagaEngine::Client::Prediction::ReconciliationBuffer<>* reconciliation_ = nullptr;
    SagaEngine::Client::InterpolationManager*              interpolation_  = nullptr;

    SnapshotFrameDecodeFn                                                   decodeFull_;
    SnapshotFrameDecodeFn                                                   decodeDelta_;
    SagaEngine::Client::Prediction::ReconciliationBuffer<>::ReplayFn        replayFn_;

    ReplicationApplyBridgeConfig                                            config_{};
    ReplicationApplyBridgeStats                                             stats_{};

    SagaEngine::Client::Prediction::PredictedState lastReconciledState_{};
    bool                                           lastFrameHadLocalPlayer_ = false;
};

} // namespace SagaEngine::Client::Replication
