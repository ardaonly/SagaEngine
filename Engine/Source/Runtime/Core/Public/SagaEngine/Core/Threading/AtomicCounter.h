/// @file AtomicCounter.h
/// @brief Lock-free stats counter built on std::atomic with relaxed ordering.
///
/// Purpose: Hot counters like "packets received", "entities replicated",
///          "jobs executed" are updated from every worker thread but only
///          read by the stats/diagnostic reporter once per tick or once per
///          second.  A mutex is overkill here — `std::atomic::fetch_add`
///          with `memory_order_relaxed` is the right primitive because the
///          counters do not synchronise with any other data.
///
/// Design:
///   - Each counter lives on its own cache line via `SAGA_CACHE_ALIGNED` to
///     avoid false sharing with neighbouring counters.
///   - `Add` / `Increment` use `memory_order_relaxed` — the fastest ordering
///     because we do not publish any other memory alongside the counter.
///   - `Load` uses `memory_order_relaxed` too; stats reporting tolerates a
///     slightly stale read in exchange for zero cost.
///   - `Reset` uses `memory_order_release` so any subsequent `Load` sees the
///     cleared value.  Reset happens rarely (tick boundary) so the stronger
///     ordering is free in practice.

#pragma once

#include "SagaEngine/Core/Threading/CacheLine.h"

#include <atomic>
#include <cstdint>

namespace SagaEngine::Core::Threading
{

// ─── AtomicCounter ───────────────────────────────────────────────────────────

/// Lock-free 64-bit unsigned counter.  Cache-line aligned so neighbouring
/// counters do not thrash the coherency fabric.
class SAGA_CACHE_ALIGNED AtomicCounter
{
public:
    /// Zero-initialised on construction.
    AtomicCounter() noexcept = default;

    /// Not copyable — copying a live counter between threads is almost
    /// always a bug, and `std::atomic` is non-copyable anyway.
    AtomicCounter(const AtomicCounter&)            = delete;
    AtomicCounter& operator=(const AtomicCounter&) = delete;

    /// Add `delta` to the counter.  `memory_order_relaxed` because the
    /// counter does not publish any other memory state alongside itself.
    void Add(std::uint64_t delta) noexcept
    {
        value_.fetch_add(delta, std::memory_order_relaxed);
    }

    /// Convenience — equivalent to `Add(1)`.
    void Increment() noexcept
    {
        value_.fetch_add(1, std::memory_order_relaxed);
    }

    /// Take a snapshot of the current value.  The value may increase again
    /// immediately after this call returns; callers should treat the result
    /// as "at least this many events happened by now".
    [[nodiscard]] std::uint64_t Load() const noexcept
    {
        return value_.load(std::memory_order_relaxed);
    }

    /// Reset the counter to zero.  Uses release ordering so a subsequent
    /// relaxed load from any thread observes the clear.  Call only from the
    /// stats reporter at a known quiescent point (e.g. tick boundary).
    void Reset() noexcept
    {
        value_.store(0, std::memory_order_release);
    }

    /// Atomically swap the stored value with zero and return the previous
    /// reading.  This is the "read-and-clear" pattern used by per-tick rate
    /// reports so counters never double-count across tick boundaries.
    [[nodiscard]] std::uint64_t Exchange() noexcept
    {
        return value_.exchange(0, std::memory_order_acq_rel);
    }

private:
    std::atomic<std::uint64_t> value_{0};
};

// ─── Size/alignment guarantees ───────────────────────────────────────────────

// A counter must occupy exactly one cache line so that arrays of counters
// line up perfectly against the L1D geometry.
static_assert(sizeof(AtomicCounter) == kCacheLineSize,
              "AtomicCounter must be cache-line sized");
static_assert(alignof(AtomicCounter) == kCacheLineSize,
              "AtomicCounter must be cache-line aligned");

} // namespace SagaEngine::Core::Threading
