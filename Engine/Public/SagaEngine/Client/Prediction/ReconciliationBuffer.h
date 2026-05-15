/// @file ReconciliationBuffer.h
/// @brief Client-side prediction history with server-authoritative reconciliation.
///
/// Layer  : Engine / Client / Prediction
/// Purpose: Client-side prediction is the technique that hides network
///          latency by executing the local player's inputs immediately
///          against a locally simulated copy of their character, while
///          still letting the server remain authoritative.  When the
///          server's snapshot arrives it includes the `lastAckedInputSeq`
///          it consumed.  The client must then:
///            1. Discard every predicted state older than the ack.
///            2. Compare its own predicted state at ack time against the
///               server's authoritative state.
///            3. If they disagree beyond an error threshold, rewind to
///               the server state and replay every pending input that
///               the server hasn't seen yet.
///
///          `ReconciliationBuffer` is the data structure that makes this
///          loop tractable: a ring of (inputSeq → predicted state)
///          records plus a replay helper.
///
/// Design:
///   - Fixed-size ring so the allocation is paid once at startup.
///     Default capacity holds ~1 second of inputs at 60 Hz.
///   - Records are keyed by a monotonically increasing `inputSeq`,
///     which must match the sequence number the client stamped on
///     its outgoing MoveCommand packet.
///   - `AcknowledgeAndReconcile` wraps the whole rewind/replay loop
///     into one call, with a user-supplied replay functor.  This keeps
///     the buffer agnostic to what "state" actually means (a
///     TransformComponent, a character controller, a physics proxy).
///   - `PositionErrorThresholdSq` is squared to avoid an unnecessary
///     sqrt on every reconcile.
///
/// Threading:
///   Not thread-safe.  The client prediction loop runs on the main
///   thread; the networking thread should hand acks off via a
///   lock-free queue before calling Acknowledge.

#pragma once

#include "SagaEngine/Math/Vec3.h"
#include "SagaEngine/Math/Quat.h"

#include <array>
#include <cstdint>
#include <functional>
#include <optional>

namespace SagaEngine::Client::Prediction {

// ─── PredictedState ─────────────────────────────────────────────────────────

/// Snapshot of what the client-side predictor thought the player state
/// looked like *right after* processing input `inputSeq`.  Stays tiny so
/// a whole second of history fits in a few kilobytes of ring storage.
struct PredictedState
{
    std::uint32_t     inputSeq    = 0;    ///< Monotonic sequence # of the consumed input.
    SagaEngine::Math::Vec3 position{};    ///< Predicted world-space position.
    SagaEngine::Math::Quat rotation = SagaEngine::Math::Quat::Identity();
    SagaEngine::Math::Vec3 velocity{};    ///< Needed for delta-time replay integration.
};

/// Authoritative server state delivered in a snapshot ack.  Kept
/// identical to PredictedState for now, but defined separately so we can
/// add server-only fields later (e.g. latency stamp, authoritative
/// cooldowns) without confusing the client predictor's internal state.
struct AuthoritativeState
{
    std::uint32_t     ackedInputSeq = 0;
    SagaEngine::Math::Vec3 position{};
    SagaEngine::Math::Quat rotation = SagaEngine::Math::Quat::Identity();
    SagaEngine::Math::Vec3 velocity{};
};

// ─── ReconciliationBuffer ───────────────────────────────────────────────────

/// Ring buffer of predicted states, oldest-to-newest by inputSeq.  The
/// capacity is a compile-time template argument so tests can use a tiny
/// buffer without paying for the default 64-entry allocation.
template <std::size_t Capacity = 128>
class ReconciliationBuffer
{
public:
    static_assert(Capacity > 0, "ReconciliationBuffer must hold at least one entry");

    /// Callback fired once per pending input during replay.  `dtSeconds`
    /// is the client's fixed time step.  The caller integrates the
    /// input for that step into the mutable `stateOut`.
    using ReplayFn = std::function<void(std::uint32_t inputSeq,
                                         float        dtSeconds,
                                         PredictedState& stateOut)>;

    ReconciliationBuffer() = default;

    // ── Prediction side ───────────────────────────────────────────────────

    /// Record what the predictor believes *right after* applying the
    /// input with sequence `inputSeq`.  If the ring is full, the oldest
    /// entry is dropped — this is the expected behaviour because older
    /// states can no longer be the subject of an ack.
    void Record(const PredictedState& state) noexcept
    {
        buffer_[head_] = state;
        head_ = (head_ + 1) % Capacity;
        if (size_ < Capacity)
            ++size_;
    }

    /// How many prediction records currently live in the ring.
    [[nodiscard]] std::size_t Size() const noexcept { return size_; }

    /// Empty the ring — used on respawn / zone transfer / rejoin when
    /// any prior history becomes meaningless.
    void Clear() noexcept
    {
        size_ = 0;
        head_ = 0;
    }

    /// Lookup the record for a given `inputSeq`, if still in the ring.
    /// Linear scan — the ring is small (<= 128 entries) so this is
    /// cheaper than maintaining a secondary index.
    [[nodiscard]] std::optional<PredictedState> Find(std::uint32_t inputSeq) const noexcept
    {
        for (std::size_t i = 0; i < size_; ++i)
        {
            const auto idx = (head_ + Capacity - size_ + i) % Capacity;
            if (buffer_[idx].inputSeq == inputSeq)
                return buffer_[idx];
        }
        return std::nullopt;
    }

    // ── Reconcile side ────────────────────────────────────────────────────

    /// Handle an incoming server ack.  Steps:
    ///   1. Drop every record with inputSeq <= ack.ackedInputSeq.
    ///   2. Compare our record at ack against the server's state.
    ///   3. If error > threshold, rewind to the server state and
    ///      replay every *remaining* (un-acked) input in the ring via
    ///      `replay`, returning the final replayed state.
    ///
    /// Returns the state the caller should adopt as "current".  If no
    /// correction was needed, this is simply the ring's newest entry.
    /// If the ring is empty after ack pruning, returns the server
    /// state unchanged (happens when the client stalls).
    PredictedState AcknowledgeAndReconcile(
        const AuthoritativeState& ack,
        float                     fixedDtSeconds,
        float                     positionErrorThresholdSq,
        const ReplayFn&           replay)
    {
        // ── Step 1: prune acked history. ──────────────────────────────────
        while (size_ > 0)
        {
            const auto oldestIdx = (head_ + Capacity - size_) % Capacity;
            if (buffer_[oldestIdx].inputSeq > ack.ackedInputSeq)
                break;
            --size_;
        }

        // If the ring is now empty the client has no pending predictions;
        // snapshot-applied state is authoritative and there's nothing to
        // reconcile against.
        if (size_ == 0)
        {
            PredictedState s;
            s.inputSeq = ack.ackedInputSeq;
            s.position = ack.position;
            s.rotation = ack.rotation;
            s.velocity = ack.velocity;
            return s;
        }

        // ── Step 2: find the predicted state *at* the ack moment. ────────
        // The oldest remaining record is the first prediction the server
        // hasn't yet confirmed.  We reconstruct "our state at ack time"
        // as the server state itself (that's by definition what the
        // server believes at that tick).  The error we care about is
        // whether *our* stale prediction drifted from what the server
        // ultimately said — which is exactly the delta below.
        const auto oldestIdx = (head_ + Capacity - size_) % Capacity;
        const auto  delta      = buffer_[oldestIdx].position - ack.position;
        const float errorSq    = delta.x * delta.x + delta.y * delta.y + delta.z * delta.z;

        const bool needsRewind = errorSq > positionErrorThresholdSq;

        if (!needsRewind)
        {
            // Our prediction was close enough — keep the latest state.
            const auto newestIdx = (head_ + Capacity - 1) % Capacity;
            return buffer_[newestIdx];
        }

        // ── Step 3: rewind + replay. ─────────────────────────────────────
        // Start from the authoritative state and walk every pending
        // input again, letting the caller integrate movement.  We mutate
        // the ring in place so future Find() queries see the corrected
        // predictions, not the stale ones.
        PredictedState cur;
        cur.inputSeq = ack.ackedInputSeq;
        cur.position = ack.position;
        cur.rotation = ack.rotation;
        cur.velocity = ack.velocity;

        for (std::size_t i = 0; i < size_; ++i)
        {
            const auto idx = (head_ + Capacity - size_ + i) % Capacity;
            const auto seq = buffer_[idx].inputSeq;
            replay(seq, fixedDtSeconds, cur);
            cur.inputSeq  = seq;
            buffer_[idx]  = cur;
        }

        return cur;
    }

private:
    std::array<PredictedState, Capacity> buffer_{};
    std::size_t                          head_ = 0;   ///< Insertion point (next Record writes here).
    std::size_t                          size_ = 0;   ///< Live entry count, ≤ Capacity.
};

} // namespace SagaEngine::Client::Prediction
