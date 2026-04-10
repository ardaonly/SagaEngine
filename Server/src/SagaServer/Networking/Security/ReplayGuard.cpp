/// @file ReplayGuard.cpp
/// @brief Sliding-window replay guard implementation.

#include "SagaServer/Networking/Security/ReplayGuard.h"

namespace SagaEngine::Networking::Security
{

// ─── Configuration ────────────────────────────────────────────────────────────

void ReplayGuard::Configure(std::int64_t stalenessMs,
                            std::int64_t futureSkewMs) noexcept
{
    // Clamp to non-negative; a negative value would disable the check and
    // silently weaken security.  Ideally caller should pass validated values.
    stalenessMs_  = stalenessMs  < 0 ? 0 : stalenessMs;
    futureSkewMs_ = futureSkewMs < 0 ? 0 : futureSkewMs;
}

// ─── Reset ───────────────────────────────────────────────────────────────────

void ReplayGuard::Reset() noexcept
{
    windowMask_ = 0;
    highestSeq_ = 0;
    lastTimeMs_ = 0;
    accepted_   = 0;
    rejected_   = 0;
}

// ─── Accept ──────────────────────────────────────────────────────────────────

ReplayDecision ReplayGuard::Accept(std::uint64_t sequence,
                                   std::int64_t  timestampMs,
                                   std::int64_t  serverNowMs) noexcept
{
    // ── Future-skew check ────────────────────────────────────────────────────
    // A packet claiming to be "in the future" beyond the allowed skew is
    // either a severe clock-sync bug or active forgery.  Reject first so a
    // forged future timestamp cannot poison the sliding window.
    if (timestampMs > serverNowMs + futureSkewMs_)
    {
        ++rejected_;
        return ReplayDecision::FutureTimestamp;
    }

    // ── Staleness check ──────────────────────────────────────────────────────
    // Packet must not be older than (last accepted - staleness).  Subtraction
    // is done in signed 64-bit so it wraps safely in either direction.
    if (lastTimeMs_ != 0 && timestampMs < lastTimeMs_ - stalenessMs_)
    {
        ++rejected_;
        return ReplayDecision::StaleTimestamp;
    }

    // ── Sliding sequence window ──────────────────────────────────────────────
    if (sequence > highestSeq_)
    {
        // Advance the window — shift the mask left by the gap, then mark
        // the new highest bit.  Large gaps clear the window entirely.
        const std::uint64_t gap = sequence - highestSeq_;
        windowMask_  = (gap >= kWindowSize) ? 0ULL : (windowMask_ << gap);
        windowMask_ |= 1ULL;  // bit 0 represents the just-accepted sequence
        highestSeq_  = sequence;
        lastTimeMs_  = timestampMs;
        ++accepted_;
        return ReplayDecision::Accept;
    }

    // sequence <= highestSeq_: check if it is inside the window and unseen.
    const std::uint64_t offset = highestSeq_ - sequence;
    if (offset >= kWindowSize)
    {
        // Too old to possibly be fresh.
        ++rejected_;
        return ReplayDecision::DuplicateSequence;
    }

    const std::uint64_t bit = 1ULL << offset;
    if ((windowMask_ & bit) != 0ULL)
    {
        // Already accepted once — this is the replay.
        ++rejected_;
        return ReplayDecision::DuplicateSequence;
    }

    // Within the window and not yet seen — accept and mark the bit.  We do
    // NOT update lastTimeMs_ here because out-of-order packets should not
    // move the staleness cursor backwards.
    windowMask_ |= bit;
    ++accepted_;
    return ReplayDecision::Accept;
}

} // namespace SagaEngine::Networking::Security
