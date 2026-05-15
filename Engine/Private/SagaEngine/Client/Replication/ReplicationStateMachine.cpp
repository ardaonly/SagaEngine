/// @file ReplicationStateMachine.cpp
/// @brief State machine governing client-side replication lifecycle.

#include "SagaEngine/Client/Replication/ReplicationStateMachine.h"

#include "SagaEngine/Core/Log/Log.h"

#include <algorithm>

namespace SagaEngine::Client::Replication {

namespace {

constexpr const char* kLogTag = "Replication";

} // namespace

// ─── State to string ───────────────────────────────────────────────────────

const char* StateToString(ReplicationState state) noexcept
{
    switch (state)
    {
        case ReplicationState::Boot:                 return "Boot";
        case ReplicationState::AwaitingFullSnapshot: return "AwaitingFullSnapshot";
        case ReplicationState::SyncingBaseline:      return "SyncingBaseline";
        case ReplicationState::Synced:               return "Synced";
        case ReplicationState::Desynced:             return "Desynced";
        case ReplicationState::ResyncRequested:      return "ResyncRequested";
        default:                                     return "Unknown";
    }
}

// ─── Sequence window ───────────────────────────────────────────────────────

bool SequenceWindow::Record(std::uint64_t seq) noexcept
{
    if (seq > highestReceived_)
    {
        // New highest — shift window.
        std::uint64_t delta = seq - highestReceived_;
        if (delta >= kWindowSize)
        {
            // Jumped past entire window — reset.
            windowMask_ = 1;
        }
        else
        {
            // Shift by delta positions.
            windowMask_ <<= static_cast<std::uint32_t>(delta);
            windowMask_ |= 1;
        }
        highestReceived_ = seq;
        return true;
    }
    else if (seq < highestReceived_)
    {
        std::uint64_t age = highestReceived_ - seq;
        if (age < kWindowSize)
        {
            std::uint32_t bit = static_cast<std::uint32_t>(age);
            bool wasSet = (windowMask_ & (std::uint64_t{1} << bit)) != 0;
            windowMask_ |= (std::uint64_t{1} << bit);
            return !wasSet;  // true if this was a new receipt.
        }
    }

    // Duplicate of highestReceived_.
    if (seq == highestReceived_)
        return false;

    // Too old to track.
    return false;
}

bool SequenceWindow::IsSeen(std::uint64_t seq) const noexcept
{
    if (seq > highestReceived_)
        return false;  // Future sequence, not seen yet.

    if (seq == highestReceived_)
        return highestReceived_ != 0;  // Only true if we've seen something.

    std::uint64_t age = highestReceived_ - seq;
    if (age >= kWindowSize)
        return false;  // Too old to track.

    std::uint32_t bit = static_cast<std::uint32_t>(age);
    return (windowMask_ & (std::uint64_t{1} << bit)) != 0;
}

std::uint32_t SequenceWindow::GapCount() const noexcept
{
    if (highestReceived_ == 0)
        return 0;

    std::uint32_t count = 0;
    std::uint64_t mask = windowMask_;

    // Count zeros in the window (missing sequences).
    for (std::uint32_t i = 1; i < kWindowSize && (highestReceived_ - i) > 0; ++i)
    {
        if ((mask & (std::uint64_t{1} << i)) == 0)
            ++count;
    }

    return count;
}

std::uint32_t SequenceWindow::GapThreshold(float rttMs) const noexcept
{
    // Dynamic threshold: max(8, 2 + rttMs / 50).
    std::uint32_t dynamic = 2 + static_cast<std::uint32_t>(rttMs / 50.0f);
    return (dynamic > 8) ? dynamic : 8;
}

bool SequenceWindow::IsDesynced(float rttMs) const noexcept
{
    return GapCount() > GapThreshold(rttMs);
}

void SequenceWindow::Reset() noexcept
{
    highestReceived_ = 0;
    windowMask_ = 0;
}

// ─── RTT estimator ─────────────────────────────────────────────────────────

void RttEstimator::AddSample(float rttMs) noexcept
{
    if (rttMs_ < 0.001f)
    {
        // First sample — use it directly.
        rttMs_ = rttMs;
    }
    else
    {
        // EMA: rtt = alpha * new + (1 - alpha) * old.
        rttMs_ = kAlpha * rttMs + (1.0f - kAlpha) * rttMs_;
    }
}

// ─── State machine ─────────────────────────────────────────────────────────

void ReplicationStateMachine::Configure(StateMachineConfig config) noexcept
{
    config_ = config;
}

void ReplicationStateMachine::TransitionTo(ReplicationState newState) noexcept
{
    if (state_ == newState)
        return;

    LOG_INFO(kLogTag,
             "Replication state: %s → %s",
             StateToString(state_),
             StateToString(newState));

    ReplicationState oldState = state_;
    state_ = newState;

    // Reset per-state counters on transition.
    if (newState == ReplicationState::Synced)
    {
        sequenceWindow_.Reset();
    }

    if (newState == ReplicationState::Desynced)
    {
        desyncEnteredTick_ = lastHashCheckTick_;  // use current tick tracking.
    }

    // If entering ResyncRequested, fire the callback.
    if (newState == ReplicationState::ResyncRequested ||
        (newState == ReplicationState::AwaitingFullSnapshot &&
         oldState == ReplicationState::Desynced))
    {
        ++stats_.resyncRequests;
        if (requestFullCb_)
            requestFullCb_();
    }
}

AcceptResult ReplicationStateMachine::AcceptPacket(ServerTick tick, bool isFullSnapshot) const noexcept
{
    (void)tick;  // Used for logging in some states.

    switch (state_)
    {
        case ReplicationState::Boot:
            return isFullSnapshot ? AcceptResult::Accept : AcceptResult::Drop;

        case ReplicationState::AwaitingFullSnapshot:
            if (isFullSnapshot)
                return AcceptResult::Accept;
            else
                return AcceptResult::Buffer;  // Buffer deltas until full snapshot applied.

        case ReplicationState::SyncingBaseline:
            if (isFullSnapshot)
                return AcceptResult::Accept;  // Newer full snapshot supersedes.
            else
                return AcceptResult::Buffer;  // Hold deltas until baseline synced.

        case ReplicationState::Synced:
            if (isFullSnapshot)
                return AcceptResult::Accept;  // Force resync with new full snapshot.
            else
                return AcceptResult::Accept;  // Normal delta flow.

        case ReplicationState::Desynced:
            if (isFullSnapshot)
                return AcceptResult::Accept;  // Full snapshot can heal desync.
            else
                return AcceptResult::Drop;    // Drop deltas while desynced.

        case ReplicationState::ResyncRequested:
            if (isFullSnapshot)
                return AcceptResult::Accept;  // Awaiting recovery snapshot.
            else
                return AcceptResult::Drop;

        default:
            return AcceptResult::Drop;
    }
}

bool ReplicationStateMachine::IsSequenceSeen(std::uint64_t seq) const noexcept
{
    return sequenceWindow_.IsSeen(seq);
}

void ReplicationStateMachine::RecordSequence(std::uint64_t seq) noexcept
{
    (void)sequenceWindow_.Record(seq);
    ++stats_.packetsAccepted;

    if (sequenceWindow_.IsDesynced(rttEstimator_.Current()))
    {
        ++stats_.gapEvents;
        if (state_ == ReplicationState::Synced)
        {
            LOG_WARN(kLogTag,
                     "Replication: sequence gap exceeded (%u missing, threshold=%u, RTT=%.1fms) — entering Desynced",
                     static_cast<unsigned>(sequenceWindow_.GapCount()),
                     static_cast<unsigned>(sequenceWindow_.GapThreshold(rttEstimator_.Current())),
                     rttEstimator_.Current());
            TransitionTo(ReplicationState::Desynced);
        }
    }
}

void ReplicationStateMachine::UpdateRtt(float rttMs) noexcept
{
    rttEstimator_.AddSample(rttMs);
    stats_.currentRttMs = rttEstimator_.Current();
}

void ReplicationStateMachine::Tick(ServerTick currentTick) noexcept
{
    // Stale patch rate tracking (per-second window).
    if (currentTick - stalePatchWindowStart_ >= 60)  // ~1 second at 60Hz.
    {
        if (stalePatchCountThisSecond_ > config_.stalePatchRateThreshold &&
            state_ == ReplicationState::Synced)
        {
            LOG_WARN(kLogTag,
                     "Replication: stale patch rate %u/sec exceeds threshold %u — triggering resync",
                     static_cast<unsigned>(stalePatchCountThisSecond_),
                     static_cast<unsigned>(config_.stalePatchRateThreshold));
            TransitionTo(ReplicationState::Desynced);
        }
        stalePatchCountThisSecond_ = 0;
        stalePatchWindowStart_ = currentTick;
    }

    // Desync grace period: auto-request full snapshot after grace ticks.
    if (state_ == ReplicationState::Desynced)
    {
        ++stats_.desyncEvents;
        std::uint64_t ticksInDesync = currentTick - desyncEnteredTick_;
        if (ticksInDesync >= config_.desyncGraceTicks)
        {
            TransitionTo(ReplicationState::ResyncRequested);
        }
    }

    // Periodic world hash check (only in Synced state).
    if (state_ == ReplicationState::Synced &&
        config_.hashCheckIntervalTicks > 0 &&
        currentTick - lastHashCheckTick_ >= config_.hashCheckIntervalTicks)
    {
        lastHashCheckTick_ = currentTick;
        ++stats_.hashChecks;

        // Hash comparison requires server cooperation — the server
        // carries its world hash in the delta snapshot header.  The
        // client computes its own hash and compares.  If they differ,
        // flag desync.
        // The actual hash comparison happens in the pipeline's Tick
        // method which has access to both client WorldState and the
        // server hash from the latest delta.  Here we just note that
        // a check was due.
    }
}

void ReplicationStateMachine::SetRequestFullSnapshotCallback(RequestFullSnapshotFn cb) noexcept
{
    requestFullCb_ = std::move(cb);
}

void ReplicationStateMachine::SetOnDesyncDetectedCallback(OnDesyncDetectedFn cb) noexcept
{
    onDesyncCb_ = std::move(cb);
}

void ReplicationStateMachine::Reset() noexcept
{
    state_ = ReplicationState::Boot;
    sequenceWindow_.Reset();
    rttEstimator_.Reset();
    lastHashCheckTick_ = 0;
    desyncEnteredTick_ = 0;
    stalePatchCountThisSecond_ = 0;
    stalePatchWindowStart_ = 0;
    stats_ = {};
}

} // namespace SagaEngine::Client::Replication
