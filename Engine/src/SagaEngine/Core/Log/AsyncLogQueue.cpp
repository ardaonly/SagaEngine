/// @file AsyncLogQueue.cpp
/// @brief SPSC ring implementation for the async logging pipeline.

#include "SagaEngine/Core/Log/AsyncLogQueue.h"

#include <algorithm>

namespace SagaEngine::Core::Log {

namespace {

// ─── Helpers ────────────────────────────────────────────────────────────────

/// True when `n` is a power of two (and non-zero).  Using this gate in
/// the constructor lets us catch a misconfigured capacity at startup,
/// not at the first enqueue/drain cycle where the bug would surface as
/// garbled messages or an infinite loop.
[[nodiscard]] constexpr bool IsPowerOfTwo(std::size_t n) noexcept
{
    return n != 0 && (n & (n - 1)) == 0;
}

} // namespace

// ─── Construction ───────────────────────────────────────────────────────────

AsyncLogQueue::AsyncLogQueue(std::size_t capacity)
{
    // Reject bad sizes by leaving the queue in a disabled state.  The
    // Enqueue/DrainTo paths early-out on `capacity_ == 0`, and callers
    // can detect the failure via Capacity().  An exception would be
    // more dramatic but this code path runs at engine startup where
    // throwing tangles error handling in every subsystem that owns a
    // queue.
    if (!IsPowerOfTwo(capacity))
        return;

    slots_        = std::make_unique<AsyncLogMessage[]>(capacity);
    capacity_     = capacity;
    capacityMask_ = capacity - 1;
}

// ─── Enqueue ────────────────────────────────────────────────────────────────

bool AsyncLogQueue::Enqueue(std::uint8_t level, std::string_view text) noexcept
{
    if (capacity_ == 0)
        return false;

    // Load the producer's own tail with `relaxed` — there is no other
    // writer.  The consumer's head needs `acquire` so we see its
    // latest advance before deciding whether there is room.
    const std::uint64_t tail = tail_.load(std::memory_order_relaxed);
    const std::uint64_t head = head_.load(std::memory_order_acquire);

    if ((tail - head) >= capacity_)
    {
        // Ring is full — bump the drop counter and bail.  Losing a
        // line is strictly preferable to blocking the producer on I/O;
        // operators get a visible signal via `droppedCount` in stats.
        dropped_.fetch_add(1, std::memory_order_relaxed);
        return false;
    }

    // Compute the slot via bitmask — `capacity_` is guaranteed to be
    // a power of two so `& capacityMask_` is equivalent to `% capacity_`.
    AsyncLogMessage& slot = slots_[tail & capacityMask_];

    // Truncate rather than reject.  A 2 KiB stack trace is still
    // useful at 1 KiB, and the producer has already burned the cost
    // of formatting it.
    const std::size_t copyLen =
        std::min(text.size(), kAsyncLogMaxMessageBytes);
    std::memcpy(slot.bytes.data(), text.data(), copyLen);
    slot.length = static_cast<std::uint16_t>(copyLen);
    slot.level  = level;

    // `release` on the tail store publishes the slot contents to the
    // consumer.  Any acquire-load of `tail_` on the consumer side that
    // sees this value is guaranteed to see the memcpy above.
    tail_.store(tail + 1, std::memory_order_release);
    enqueued_.fetch_add(1, std::memory_order_relaxed);
    return true;
}

// ─── DrainTo ────────────────────────────────────────────────────────────────

std::size_t AsyncLogQueue::DrainTo(const Sink& sink, std::size_t maxMessages) noexcept
{
    if (capacity_ == 0 || !sink)
        return 0;

    // Consumer is the sole writer of `head_` so a relaxed self-load is
    // enough.  `tail_` is written by the producer, so we need acquire.
    std::uint64_t head = head_.load(std::memory_order_relaxed);
    const std::uint64_t tail = tail_.load(std::memory_order_acquire);

    // `maxMessages == 0` means "drain everything the producer has
    // published".  That mode is used at shutdown when the producer is
    // already quiesced; mid-tick the consumer passes a cap so it
    // doesn't monopolise disk bandwidth during a burst.
    const std::uint64_t available = tail - head;
    const std::uint64_t target =
        (maxMessages == 0)
            ? available
            : std::min<std::uint64_t>(available, maxMessages);

    std::size_t drained = 0;
    for (std::uint64_t i = 0; i < target; ++i)
    {
        AsyncLogMessage& slot = slots_[head & capacityMask_];
        sink(slot.level,
             std::string_view(slot.bytes.data(), slot.length));
        ++head;
        ++drained;
    }

    if (drained > 0)
    {
        // Release-store on the head lets the producer observe the
        // freed slots.  Skipping the store when drained == 0 keeps
        // the fast path of a quiet consumer from touching the line
        // at all.
        head_.store(head, std::memory_order_release);
        drained_.fetch_add(drained, std::memory_order_relaxed);
    }

    return drained;
}

// ─── Snapshot ───────────────────────────────────────────────────────────────

AsyncLogQueueStats AsyncLogQueue::Snapshot() const noexcept
{
    // Relaxed everywhere — snapshots are diagnostic, not a barrier.
    // Readers of stats should treat small inconsistencies as noise.
    AsyncLogQueueStats out;
    out.enqueuedCount = enqueued_.load(std::memory_order_relaxed);
    out.drainedCount  = drained_.load(std::memory_order_relaxed);
    out.droppedCount  = dropped_.load(std::memory_order_relaxed);
    out.capacity      = static_cast<std::uint32_t>(capacity_);

    const std::uint64_t head = head_.load(std::memory_order_relaxed);
    const std::uint64_t tail = tail_.load(std::memory_order_relaxed);
    out.currentDepth = static_cast<std::uint32_t>(tail - head);
    return out;
}

} // namespace SagaEngine::Core::Log
