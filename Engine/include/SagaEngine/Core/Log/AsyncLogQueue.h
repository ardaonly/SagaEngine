/// @file AsyncLogQueue.h
/// @brief Lock-free ring buffer that hands log lines to a dedicated writer thread.
///
/// Layer  : SagaEngine / Core / Log
/// Purpose: `Log.cpp` writes to its sinks synchronously.  That is fine
///          for development where the volume is low, but it is a
///          latency bomb on a production tick: a blocking `fsync` on a
///          slow disk during a crowded raid can stall the simulation
///          thread for tens of milliseconds.  Any logging path that
///          runs on the hot loop must be non-blocking or the tick rate
///          becomes a function of disk load.
///
///          `AsyncLogQueue` is the solution: a fixed-capacity single-
///          producer / single-consumer ring buffer.  Producers (game
///          threads) enqueue a prefabricated message; the consumer (a
///          dedicated thread inside LogRotationSink or similar) drains
///          the ring at whatever rate the disk allows.  The producer
///          never blocks — if the ring is full the message is dropped
///          and a `droppedCount` counter increments so operators know
///          to size the ring up.
///
/// Design rules:
///   - Single producer, single consumer.  Multiple threads that want
///     to log concurrently do so through the higher-level `Log` facade
///     which already serialises calls.  Avoiding MPSC machinery keeps
///     this class tiny and branch-predictable.
///   - Power-of-two capacity so the wrap is a bitmask and not a modulo.
///     A `static_assert` in the ctor enforces this at compile time.
///   - Messages have a bounded size (`kMaxMessageBytes`).  Logging is
///     not the place to ship multi-megabyte payloads; anything larger
///     should go through a dedicated channel.
///
/// Threading model:
///   - Producer calls `Enqueue` from any ONE thread.  Tail index uses
///     release semantics; consumer load uses acquire.
///   - Consumer calls `DrainTo` from exactly ONE thread — the dedicated
///     log writer thread owned by the log subsystem.  Head index uses
///     release semantics; producer load uses acquire.
///   - No mutex.  The atomics are the only synchronisation.
///
/// What this header is NOT:
///   - Not a general-purpose queue.  The strict SPSC invariant is part
///     of the type contract; breaking it will corrupt the ring.
///   - Not an I/O layer.  It moves bytes from producers to a consumer
///     thread; the consumer decides where they go (file, UDP, syslog).

#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <memory>
#include <string_view>

namespace SagaEngine::Core::Log {

// ─── Tunables ───────────────────────────────────────────────────────────────

/// Maximum bytes in a single log message, including a trailing newline
/// but excluding any sink-side prefix (timestamp, category).  Matches
/// the fixed buffer `Log.cpp` uses to format entries, so the producer
/// can copy straight in without reallocating.
inline constexpr std::size_t kAsyncLogMaxMessageBytes = 1024;

/// Default ring capacity — must be a power of two.  4096 slots holds
/// roughly four seconds of chatter at 1000 lines/s, which is enough
/// headroom to ride out a disk hiccup without dropping anything.
inline constexpr std::size_t kAsyncLogDefaultCapacity = 4096;

// ─── Message slot ───────────────────────────────────────────────────────────

/// One ring slot.  Inline storage keeps the whole ring in one
/// contiguous allocation and lets the consumer read without touching
/// the heap.  `length` is zero for an empty slot; `bytes` is not
/// null-terminated — the consumer treats it as length-counted.
struct AsyncLogMessage
{
    std::uint16_t                                 length = 0;
    std::uint8_t                                  level  = 0; ///< LogLevel enum, passed through.
    std::uint8_t                                  padding = 0;
    std::array<char, kAsyncLogMaxMessageBytes>    bytes{};
};

// ─── Statistics ─────────────────────────────────────────────────────────────

/// Counters snapshot.  Useful for `/logstat` and for sizing the ring at
/// deployment time — if `droppedCount` is ever non-zero, the ring is
/// too small for the observed peak rate and must be grown.
struct AsyncLogQueueStats
{
    std::uint64_t enqueuedCount = 0;
    std::uint64_t drainedCount  = 0;
    std::uint64_t droppedCount  = 0;
    std::uint32_t currentDepth  = 0;
    std::uint32_t capacity      = 0;
};

// ─── Ring buffer ────────────────────────────────────────────────────────────

/// Fixed-capacity SPSC ring for log messages.  Capacity is supplied at
/// construction and must be a power of two; the class will refuse to
/// start otherwise (returns an empty queue you can detect via
/// `Capacity() == 0`).
class AsyncLogQueue
{
public:
    explicit AsyncLogQueue(std::size_t capacity = kAsyncLogDefaultCapacity);
    ~AsyncLogQueue() = default;

    AsyncLogQueue(const AsyncLogQueue&)            = delete;
    AsyncLogQueue& operator=(const AsyncLogQueue&) = delete;

    // ── Producer ──────────────────────────────────────────────────────────

    /// Append a message.  Returns `true` on success, `false` if the
    /// ring is full — in which case `droppedCount_` increments and the
    /// caller's line is LOST.  The producer never blocks; logging must
    /// never become a choke point on the simulation thread.
    /// `text` is truncated to `kAsyncLogMaxMessageBytes` if necessary.
    bool Enqueue(std::uint8_t level, std::string_view text) noexcept;

    // ── Consumer ──────────────────────────────────────────────────────────

    /// Handler type used by `DrainTo`.  The view is valid only for the
    /// duration of the call — the consumer must copy anything it wants
    /// to outlive the call (e.g. into a pending-flush buffer).
    using Sink = std::function<void(std::uint8_t level, std::string_view text)>;

    /// Drain up to `maxMessages` messages into the sink.  Returns the
    /// number actually drained.  Pass `0` for `maxMessages` to drain
    /// every available message in one call — handy at shutdown when
    /// the producer is quiesced.
    std::size_t DrainTo(const Sink& sink, std::size_t maxMessages = 0) noexcept;

    // ── Introspection ─────────────────────────────────────────────────────

    [[nodiscard]] std::size_t Capacity() const noexcept { return capacity_; }

    /// Point-in-time counters.  Cheap to call — all members are atomics
    /// or load from atomics, no mutex.  Safe from any thread.
    [[nodiscard]] AsyncLogQueueStats Snapshot() const noexcept;

private:
    /// Bitmask trick requires a power-of-two capacity; we stash
    /// `capacity - 1` so the wrap is a single `& capacityMask_`.
    std::size_t capacity_     = 0;
    std::size_t capacityMask_ = 0;

    /// Heap-allocated because placing several KiB of inline storage on
    /// the stack of every thread that constructs a temporary queue is
    /// wasteful.  `unique_ptr<T[]>` keeps this header RAII-clean.
    std::unique_ptr<AsyncLogMessage[]> slots_;

    /// head_ is owned by the consumer, tail_ by the producer.  Each is
    /// read by the other side with acquire semantics so the writes to
    /// the message slot happen-before the consumer observes the new
    /// tail.  Contention between producer and consumer is limited to
    /// these two cache lines.
    std::atomic<std::uint64_t> head_{0};
    std::atomic<std::uint64_t> tail_{0};

    /// Counters are release-updated by whichever side owns them:
    /// producer owns enqueued/dropped, consumer owns drained.  The
    /// snapshot takes a relaxed load; it's a stats endpoint, not a
    /// synchronisation point.
    std::atomic<std::uint64_t> enqueued_{0};
    std::atomic<std::uint64_t> dropped_{0};
    std::atomic<std::uint64_t> drained_{0};
};

} // namespace SagaEngine::Core::Log
