/// @file SnapshotApplyPipeline.cpp
/// @brief Client-side pipeline that folds server snapshots into the local ECS.

#include "SagaEngine/Client/Replication/SnapshotApplyPipeline.h"

#include "SagaEngine/Core/Log/Log.h"
#include "SagaEngine/Simulation/WorldState.h"

#include <algorithm>
#include <cstring>
#include <utility>

namespace SagaEngine::Client::Replication {

namespace {

constexpr const char* kLogTag = "Replication";

/// FNV-1a 64-bit hash for CRC32 validation.
[[nodiscard]] std::uint32_t CRC32_Fast(const std::uint8_t* data, std::size_t size) noexcept
{
    std::uint32_t crc = 0xFFFFFFFFu;
    for (std::size_t i = 0; i < size; ++i)
    {
        crc ^= static_cast<std::uint32_t>(data[i]);
        for (int bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ ((crc & 1) ? 0xEDB88320u : 0);
    }
    return crc ^ 0xFFFFFFFFu;
}

} // namespace

// ─── Configure ──────────────────────────────────────────────────────────────

bool SnapshotApplyPipeline::Configure(
    Simulation::WorldState* world,
    FullSnapshotApplyFn     applyFull,
    DeltaSnapshotApplyFn    applyDelta,
    const SnapshotPipelineConfig& config)
{
    if (!world || !applyFull || !applyDelta)
    {
        LOG_ERROR(kLogTag, "SnapshotApplyPipeline: null arguments to Configure");
        return false;
    }

    world_      = world;
    applyFull_  = std::move(applyFull);
    applyDelta_ = std::move(applyDelta);
    config_     = config;

    // Reset baseline on reconfigure.
    Reset();

    LOG_INFO(kLogTag,
             "SnapshotApplyPipeline: configured (jitterSlots=%u, maxAge=%u, autoResync=%s)",
             static_cast<unsigned>(config_.jitterBufferSlots),
             static_cast<unsigned>(config_.maxBufferedAgeTicks),
             config_.autoRequestFullSnapshotOnMiss ? "true" : "false");

    return true;
}

void SnapshotApplyPipeline::SetRequestFullSnapshotCallback(RequestFullSnapshotFn cb)
{
    requestFullCb_ = std::move(cb);
}

// ─── Submit full snapshot ──────────────────────────────────────────────────

ApplyOutcome SnapshotApplyPipeline::SubmitFull(DecodedWorldSnapshot&& snapshot)
{
    if (!world_ || !applyFull_)
        return ApplyOutcome::InternalError;

    // CRC32 verification.
    if (snapshot.crc32 != 0)
    {
        std::uint32_t computed = CRC32_Fast(snapshot.payload.data(), snapshot.payload.size());
        if (computed != snapshot.crc32)
        {
            LOG_WARN(kLogTag,
                     "SnapshotApplyPipeline: CRC mismatch on full snapshot "
                     "(expected 0x%08X, computed 0x%08X)",
                     static_cast<unsigned>(snapshot.crc32),
                     static_cast<unsigned>(computed));
            ++stats_.crcFailures;
            return ApplyOutcome::CrcMismatch;
        }
    }

    // Tick must be newer than current baseline.
    if (snapshot.serverTick <= lastAppliedTick_)
    {
        if (snapshot.serverTick == lastAppliedTick_)
            return ApplyOutcome::DroppedDuplicate;
        return ApplyOutcome::DroppedOld;
    }

    // Apply the snapshot.
    if (!applyFull_(*world_, snapshot))
    {
        LOG_ERROR(kLogTag,
                  "SnapshotApplyPipeline: full snapshot apply failed for tick %llu",
                  static_cast<unsigned long long>(snapshot.serverTick));
        return ApplyOutcome::InternalError;
    }

    lastAppliedTick_ = snapshot.serverTick;
    ++stats_.fullApplied;

    // Clear jitter buffer — all buffered deltas are now irrelevant.
    jitterBuffer_.clear();

    LOG_INFO(kLogTag,
             "SnapshotApplyPipeline: full snapshot applied tick=%llu (total full=%llu, delta=%llu)",
             static_cast<unsigned long long>(lastAppliedTick_),
             static_cast<unsigned long long>(stats_.fullApplied),
             static_cast<unsigned long long>(stats_.deltaApplied));

    return ApplyOutcome::Ok;
}

// ─── Submit delta snapshot ─────────────────────────────────────────────────

ApplyOutcome SnapshotApplyPipeline::SubmitDelta(DecodedDeltaSnapshot&& delta)
{
    if (!world_ || !applyDelta_)
        return ApplyOutcome::InternalError;

    // Baseline validation.
    if (delta.baselineTick == lastAppliedTick_)
    {
        // Baseline matches — apply immediately.
        if (!applyDelta_(*world_, delta))
        {
            LOG_ERROR(kLogTag,
                      "SnapshotApplyPipeline: delta apply failed for tick %llu "
                      "(baseline=%llu)",
                      static_cast<unsigned long long>(delta.serverTick),
                      static_cast<unsigned long long>(delta.baselineTick));
            return ApplyOutcome::InternalError;
        }

        lastAppliedTick_ = delta.serverTick;
        ++stats_.deltaApplied;

        // Try to promote buffered deltas.
        PromoteBufferedDeltas();

        return ApplyOutcome::Ok;
    }
    else if (delta.baselineTick < lastAppliedTick_)
    {
        return ApplyOutcome::DroppedOld;
    }
    else if (delta.baselineTick == delta.serverTick)
    {
        // Self-baseline (first delta after full snapshot) — apply.
        if (!applyDelta_(*world_, delta))
            return ApplyOutcome::InternalError;

        lastAppliedTick_ = delta.serverTick;
        ++stats_.deltaApplied;
        return ApplyOutcome::Ok;
    }
    else
    {
        // Baseline is in the future — buffer.
        return BufferDelta(std::move(delta));
    }
}

// ─── Tick ──────────────────────────────────────────────────────────────────

void SnapshotApplyPipeline::Tick(ServerTick currentClientBaselineHint)
{
    (void)currentClientBaselineHint;

    // Try to promote buffered deltas whose baseline has arrived.
    PromoteBufferedDeltas();

    // Evict old entries.
    EvictStaleDeltas();
}

// ─── Reset ─────────────────────────────────────────────────────────────────

void SnapshotApplyPipeline::Reset()
{
    lastAppliedTick_ = kInvalidTick;
    jitterBuffer_.clear();
    stats_ = {};
}

// ─── Internal helpers ──────────────────────────────────────────────────────

ApplyOutcome SnapshotApplyPipeline::BufferDelta(DecodedDeltaSnapshot&& delta)
{
    // Check capacity.
    if (jitterBuffer_.size() >= config_.jitterBufferSlots)
    {
        ++stats_.jitterOverflow;

        // Evict oldest entry.
        if (!jitterBuffer_.empty())
        {
            jitterBuffer_.erase(jitterBuffer_.begin());
            LOG_WARN(kLogTag,
                     "SnapshotApplyPipeline: jitter buffer overflow, evicted oldest");
        }
    }

    // Insert sorted by baselineTick (ascending).
    auto it = std::lower_bound(jitterBuffer_.begin(), jitterBuffer_.end(),
                               delta,
                               [](const DecodedDeltaSnapshot& a,
                                  const DecodedDeltaSnapshot& b) {
                                   return a.baselineTick < b.baselineTick;
                               });
    jitterBuffer_.insert(it, std::move(delta));
    ++stats_.bufferedDeltas;

    return ApplyOutcome::BufferedForOrdering;
}

void SnapshotApplyPipeline::PromoteBufferedDeltas()
{
    // Walk the jitter buffer and apply any delta whose baseline matches
    // the current lastAppliedTick.  Process in order (sorted by baseline).
    std::size_t writeIdx = 0;
    for (std::size_t readIdx = 0; readIdx < jitterBuffer_.size(); ++readIdx)
    {
        DecodedDeltaSnapshot& delta = jitterBuffer_[readIdx];

        if (delta.baselineTick == lastAppliedTick_)
        {
            // Apply this delta.
            if (applyDelta_ && world_)
            {
                if (!applyDelta_(*world_, delta))
                {
                    LOG_WARN(kLogTag,
                             "SnapshotApplyPipeline: promoted delta apply failed for tick %llu",
                             static_cast<unsigned long long>(delta.serverTick));
                    // Leave this delta in the buffer (will be retried next frame).
                    if (readIdx != writeIdx)
                        jitterBuffer_[writeIdx] = std::move(delta);
                    ++writeIdx;
                    continue;
                }

                lastAppliedTick_ = delta.serverTick;
                ++stats_.deltaApplied;
                // Delta consumed — do not increment writeIdx (it's removed).
            }
        }
        else
        {
            // Keep in buffer.
            if (readIdx != writeIdx)
                jitterBuffer_[writeIdx] = std::move(delta);
            ++writeIdx;
        }
    }

    // Shrink to new size.
    jitterBuffer_.resize(writeIdx);

    // If we still have buffered deltas but none could be promoted, and
    // auto-request is enabled, fire the recovery callback.
    if (!jitterBuffer_.empty() && config_.autoRequestFullSnapshotOnMiss)
    {
        // Check if the earliest buffered delta's baseline is unreasonably
        // far behind — this indicates a missing baseline snapshot.
        const DecodedDeltaSnapshot& earliest = jitterBuffer_.front();
        if (earliest.baselineTick < lastAppliedTick_)
        {
            ++stats_.missingBaseline;
            if (requestFullCb_)
                requestFullCb_();
        }
    }
}

void SnapshotApplyPipeline::EvictStaleDeltas()
{
    if (config_.maxBufferedAgeTicks == 0)
        return;

    jitterBuffer_.erase(
        std::remove_if(jitterBuffer_.begin(), jitterBuffer_.end(),
                       [this](const DecodedDeltaSnapshot& d) {
                           if (lastAppliedTick_ == kInvalidTick)
                               return false;
                           std::uint64_t age = (d.serverTick > lastAppliedTick_)
                                               ? (d.serverTick - lastAppliedTick_)
                                               : 0;
                           return age > config_.maxBufferedAgeTicks;
                       }),
        jitterBuffer_.end());
}

} // namespace SagaEngine::Client::Replication
