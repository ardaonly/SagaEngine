/// @file ReplicationApplyBridge.cpp
/// @brief Bridge implementation — routes decoded snapshots to
///        prediction (local player) and interpolation (remote entities).

#include "SagaEngine/Client/Replication/ReplicationApplyBridge.h"

#include "SagaEngine/Core/Log/Log.h"

#include <utility>

namespace SagaEngine::Client::Replication {

namespace {

constexpr const char* kLogTag = "Replication";

/// Floor on the server tick rate.  A zero or negative value would
/// divide-by-zero inside `TickToSeconds`; clamping protects callers
/// that forget to set the field in their configuration.
constexpr double kMinServerTickHz = 1.0;

} // namespace

// ─── Configure ────────────────────────────────────────────────────────────

bool ReplicationApplyBridge::Configure(
    SnapshotApplyPipeline&                                  pipeline,
    SagaEngine::Simulation::WorldState*                     world,
    SagaEngine::Client::Prediction::ReconciliationBuffer<>* reconciliation,
    SagaEngine::Client::InterpolationManager*               interpolation,
    SnapshotFrameDecodeFn                                   decodeFull,
    SnapshotFrameDecodeFn                                   decodeDelta,
    const ReplicationApplyBridgeConfig&                     config,
    const SnapshotPipelineConfig&                           pipelineConfig)
{
    if (world == nullptr)
    {
        LOG_ERROR(kLogTag,
                  "ReplicationApplyBridge::Configure: world is null");
        return false;
    }

    if (reconciliation == nullptr && interpolation == nullptr)
    {
        LOG_ERROR(kLogTag,
                  "ReplicationApplyBridge::Configure: both reconciliation and "
                  "interpolation are null — refusing to install");
        return false;
    }

    if (!decodeFull || !decodeDelta)
    {
        LOG_ERROR(kLogTag,
                  "ReplicationApplyBridge::Configure: decode callbacks must "
                  "both be non-null");
        return false;
    }

    pipeline_       = &pipeline;
    reconciliation_ = reconciliation;
    interpolation_  = interpolation;
    decodeFull_     = std::move(decodeFull);
    decodeDelta_    = std::move(decodeDelta);
    config_         = config;

    if (config_.serverTickHz < kMinServerTickHz)
    {
        LOG_WARN(kLogTag,
                 "ReplicationApplyBridge: serverTickHz=%.2f clamped to %.2f",
                 config_.serverTickHz,
                 kMinServerTickHz);
        config_.serverTickHz = kMinServerTickHz;
    }

    // Build the wrapper callbacks that forward every successful
    // pipeline apply into `OnFullApply` / `OnDeltaApply`.  `this`
    // capture is safe because the bridge outlives the pipeline by
    // contract (both destructed on session teardown with the pipeline
    // going first via `Reset`).
    auto fullCb = [this](SagaEngine::Simulation::WorldState& w,
                         const DecodedWorldSnapshot&         snapshot) {
        return OnFullApply(w, snapshot);
    };
    auto deltaCb = [this](SagaEngine::Simulation::WorldState& w,
                          const DecodedDeltaSnapshot&         delta) {
        return OnDeltaApply(w, delta);
    };

    // (Re)configure the pipeline with bridge-owned callbacks.  This
    // resets the pipeline's baseline, which is the intended behaviour
    // at session start — any state from before the bridge existed is
    // safely discarded.
    if (!pipeline_->Configure(world,
                              std::move(fullCb),
                              std::move(deltaCb),
                              pipelineConfig))
    {
        LOG_ERROR(kLogTag,
                  "ReplicationApplyBridge::Configure: pipeline Configure failed");
        pipeline_ = nullptr;
        return false;
    }

    LOG_INFO(kLogTag,
             "ReplicationApplyBridge: configured (localPlayer=%u, "
             "fixedDt=%.4f, serverHz=%.1f, prediction=%s, interpolation=%s)",
             static_cast<unsigned>(config_.localPlayerId),
             static_cast<double>(config_.fixedDtSeconds),
             config_.serverTickHz,
             reconciliation_ != nullptr ? "on" : "off",
             interpolation_  != nullptr ? "on" : "off");
    return true;
}

// ─── SetReplayFn ──────────────────────────────────────────────────────────

void ReplicationApplyBridge::SetReplayFn(
    SagaEngine::Client::Prediction::ReconciliationBuffer<>::ReplayFn fn)
{
    replayFn_ = std::move(fn);
}

// ─── Reset ────────────────────────────────────────────────────────────────

void ReplicationApplyBridge::Reset()
{
    stats_                   = ReplicationApplyBridgeStats{};
    lastReconciledState_     = SagaEngine::Client::Prediction::PredictedState{};
    lastFrameHadLocalPlayer_ = false;

    if (reconciliation_ != nullptr)
        reconciliation_->Clear();

    if (interpolation_ != nullptr)
        interpolation_->Clear();
}

// ─── OnFullApply ──────────────────────────────────────────────────────────

bool ReplicationApplyBridge::OnFullApply(
    SagaEngine::Simulation::WorldState& /*world*/,
    const DecodedWorldSnapshot&         snapshot)
{
    DecodedSnapshotFrame frame;
    frame.entities.reserve(snapshot.entityCount);

    if (!decodeFull_ || !decodeFull_(snapshot.payload, frame))
    {
        ++stats_.fullDecodeFailures;
        LOG_WARN(kLogTag,
                 "ReplicationApplyBridge: full snapshot decode failed at "
                 "tick=%llu (payload=%zu B)",
                 static_cast<unsigned long long>(snapshot.serverTick),
                 snapshot.payload.size());
        return false;
    }

    ++stats_.fullSnapshotsRouted;

    RouteRemoteEntities(frame, snapshot.serverTick);
    RouteLocalPlayer(frame);

    return true;
}

// ─── OnDeltaApply ─────────────────────────────────────────────────────────

bool ReplicationApplyBridge::OnDeltaApply(
    SagaEngine::Simulation::WorldState& /*world*/,
    const DecodedDeltaSnapshot&         delta)
{
    DecodedSnapshotFrame frame;
    frame.entities.reserve(delta.dirtyEntityCount);

    if (!decodeDelta_ || !decodeDelta_(delta.payload, frame))
    {
        ++stats_.deltaDecodeFailures;
        LOG_WARN(kLogTag,
                 "ReplicationApplyBridge: delta decode failed at "
                 "serverTick=%llu baseline=%llu (payload=%zu B)",
                 static_cast<unsigned long long>(delta.serverTick),
                 static_cast<unsigned long long>(delta.baselineTick),
                 delta.payload.size());
        return false;
    }

    ++stats_.deltaSnapshotsRouted;

    RouteRemoteEntities(frame, delta.serverTick);
    RouteLocalPlayer(frame);

    return true;
}

// ─── RouteRemoteEntities ──────────────────────────────────────────────────

void ReplicationApplyBridge::RouteRemoteEntities(
    const DecodedSnapshotFrame& frame,
    ServerTick                  serverTick)
{
    if (interpolation_ == nullptr)
        return;

    const double serverTimeSeconds = TickToSeconds(serverTick);
    const auto   localPlayerId     = config_.localPlayerId;

    for (const auto& entity : frame.entities)
    {
        // Skip the local player — its state goes through prediction,
        // not interpolation.  Double-compare so an unconfigured (0)
        // local player id still routes every entity into interpolation.
        if (localPlayerId != 0 && entity.entityId == localPlayerId)
            continue;

        interpolation_->PushSnapshot(entity.entityId,
                                     serverTimeSeconds,
                                     entity.transform);
        ++stats_.remoteEntitySnapshots;
    }
}

// ─── RouteLocalPlayer ─────────────────────────────────────────────────────

void ReplicationApplyBridge::RouteLocalPlayer(const DecodedSnapshotFrame& frame)
{
    lastFrameHadLocalPlayer_ = false;

    if (reconciliation_ == nullptr || config_.localPlayerId == 0)
        return;

    if (!frame.hasLocalPlayer)
        return;

    // Locate the local player's record in the decoded frame.  Linear
    // scan because the frame is small and the local player is usually
    // either the first or last entry, so a map lookup would be more
    // expensive on average.
    const SnapshotEntityState* playerState = nullptr;
    for (const auto& entity : frame.entities)
    {
        if (entity.entityId == config_.localPlayerId)
        {
            playerState = &entity;
            break;
        }
    }

    if (playerState == nullptr)
        return;

    lastFrameHadLocalPlayer_ = true;
    ++stats_.localPlayerAcks;
    stats_.lastAckedInputSeq = frame.ackedInputSeq;

    // Build the authoritative state the reconciliation buffer consumes.
    // Translation → position, rotation → rotation; velocity lives
    // outside the `Transform` type and is carried through the
    // `SnapshotEntityState` struct verbatim.
    SagaEngine::Client::Prediction::AuthoritativeState ack;
    ack.ackedInputSeq = frame.ackedInputSeq;
    ack.position      = playerState->transform.position;
    ack.rotation      = playerState->transform.rotation;
    ack.velocity      = playerState->velocity;

    // Capture the pre-reconcile ring size so we can tell whether a
    // rewind happened this tick.  The buffer does not expose a
    // "did I replay" flag, so we compare positions before and after
    // — if the caller's replay functor actually ran, the returned
    // state will differ from the authoritative state in the
    // `inputSeq` field (replay walks every pending input).
    // If the caller never installed a replay functor we still need to
    // hand the buffer *something* — the reconciler calls the functor
    // unconditionally inside its replay loop.  A no-op is safe: the
    // buffer's own book-keeping will copy the authoritative state into
    // each pending slot, which is exactly the "observer client"
    // behaviour we want.
    static const SagaEngine::Client::Prediction::ReconciliationBuffer<>::ReplayFn
        kNoopReplay = [](std::uint32_t,
                         float,
                         SagaEngine::Client::Prediction::PredictedState&) {};

    const auto  preSize = reconciliation_->Size();
    const auto  result  = reconciliation_->AcknowledgeAndReconcile(
        ack,
        config_.fixedDtSeconds,
        config_.positionErrorThresholdSq,
        replayFn_ ? replayFn_ : kNoopReplay);

    lastReconciledState_ = result;

    // Count a rewind when the buffer still had pending inputs and the
    // returned state advanced past the ack (i.e. replay ran).
    if (preSize > 0 && result.inputSeq > ack.ackedInputSeq)
        ++stats_.reconcileRewinds;
}

// ─── TickToSeconds ────────────────────────────────────────────────────────

double ReplicationApplyBridge::TickToSeconds(ServerTick tick) const noexcept
{
    // `serverTickHz` was clamped at Configure time so the division
    // is always safe.  The result is a monotonically increasing
    // "seconds-since-server-start" value that the interpolation
    // buffer treats as its time base.
    return static_cast<double>(tick) / config_.serverTickHz;
}

} // namespace SagaEngine::Client::Replication
