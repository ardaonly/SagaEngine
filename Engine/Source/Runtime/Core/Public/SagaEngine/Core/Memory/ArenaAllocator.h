/// @file ArenaAllocator.h
/// @brief Chained-block arena (linear) allocator with marker / rewind.
///
/// Layer  : SagaEngine / Core / Memory
/// Purpose: The server tick and most of the client subsystems produce
///          huge bursts of short-lived allocations — a snapshot pass,
///          an interest recomputation, a delta-encode pass.  Hitting
///          the global heap for every one of those is both slow and a
///          lock-contention risk.  The arena is the answer: allocate
///          into a monotonically growing block list, then drop the
///          whole arena (or rewind to a marker) at the end of the
///          tick and let the operating system keep the pages.
///
/// Design:
///   - Blocks are chained in allocation order.  When the head block
///     runs out of room a fresh block is appended; block sizes are
///     `max(blockSize, requestSize + alignment)` so big allocations
///     do not waste an entire block.
///   - `Marker` captures the arena's current write position.  A later
///     `Rewind(marker)` rolls every allocation made after the marker
///     back to the pool without touching heap memory.  `ScopedArena`
///     is the RAII wrapper around that pattern.
///   - `Reset` walks every block's offset back to zero but keeps the
///     block list intact — next tick re-uses the same pages with zero
///     heap churn.
///   - Non-owning references — the arena does not call destructors
///     for objects it constructed with `Create`.  Callers that need
///     destructor calls should either use trivially-destructible
///     types or manage the destructors themselves.
///
/// Thread safety:
///   Not thread-safe.  Each thread that wants arena allocation owns
///   its own `ArenaAllocator`; the typical pattern is one arena per
///   worker thread in the server's job system.
///
/// What this header is NOT:
///   - Not a general-purpose allocator.  There is no `Free(ptr)` —
///     you rewind or reset the arena, you do not free individual
///     allocations.
///   - Not a heap fallback.  An out-of-budget arena returns nullptr
///     instead of calling the global heap so the caller has an
///     explicit signal to spill.

#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <new>
#include <type_traits>
#include <utility>
#include <vector>

namespace SagaEngine::Core {

// ─── ArenaStats ───────────────────────────────────────────────────────────

/// Observable counters for diagnostics overlays and leak tests.
/// `bytesInUse` shrinks on `Rewind`; `bytesAllocated` is the peak
/// we have ever requested, so the operator can tell whether a
/// tick came close to its reserve budget.
struct ArenaStats
{
    std::size_t bytesInUse       = 0; ///< Bytes currently reserved for live allocations.
    std::size_t bytesAllocated   = 0; ///< Cumulative bytes ever requested (high-water mark).
    std::size_t bytesCommitted   = 0; ///< Sum of every block's size — i.e. heap footprint.
    std::uint32_t blockCount     = 0; ///< Number of chained blocks.
    std::uint32_t allocations    = 0; ///< Successful `Allocate` calls since last `Reset`.
    std::uint32_t allocationFailures = 0; ///< `Allocate` calls that returned nullptr.
};

// ─── ArenaAllocator ───────────────────────────────────────────────────────

/// Chained-block arena.  One instance per owner; the instance owns
/// its block list and releases everything on destruction.
class ArenaAllocator
{
public:
    /// Default block size — 1 MiB is large enough to absorb a full
    /// snapshot tick on a typical MMO server without chaining.
    static constexpr std::size_t kDefaultBlockBytes = 1u << 20;

    /// Opaque cursor into the arena.  Captured by `Mark` and passed
    /// back to `Rewind` to undo every allocation in between.  Stable
    /// across `Allocate` calls but *not* across `Reset` — resetting
    /// the arena invalidates every outstanding marker.
    struct Marker
    {
        std::uint32_t blockIndex = 0;
        std::size_t   blockOffset = 0;
    };

    explicit ArenaAllocator(std::size_t blockBytes = kDefaultBlockBytes) noexcept;

    ~ArenaAllocator() noexcept;

    ArenaAllocator(const ArenaAllocator&)            = delete;
    ArenaAllocator& operator=(const ArenaAllocator&) = delete;
    ArenaAllocator(ArenaAllocator&&)                 = delete;
    ArenaAllocator& operator=(ArenaAllocator&&)      = delete;

    // ── Allocation ────────────────────────────────────────────────────────

    /// Allocate `bytes` with `alignment` guarantees.  Returns nullptr
    /// if the request exceeds the arena budget (no heap fallback).
    /// `alignment` must be a power of two; non-powers-of-two are
    /// clamped up to the next power of two.
    [[nodiscard]] void* Allocate(
        std::size_t bytes,
        std::size_t alignment = alignof(std::max_align_t)) noexcept;

    /// Type-safe helper around `Allocate` + placement-new.  The
    /// returned pointer is *not* destroyed by the arena — if `T`
    /// has a non-trivial destructor the caller is responsible for
    /// calling `Destroy(ptr)` before the arena is reset.
    template <typename T, typename... Args>
    [[nodiscard]] T* Create(Args&&... args)
    {
        void* mem = Allocate(sizeof(T), alignof(T));
        if (mem == nullptr)
            return nullptr;
        return ::new (mem) T(std::forward<Args>(args)...);
    }

    /// Explicit destructor call.  Does not free memory — the arena
    /// reclaims the bytes on `Rewind` / `Reset`.
    template <typename T>
    void Destroy(T* ptr) noexcept
    {
        if (ptr != nullptr)
            ptr->~T();
    }

    // ── Marker / rewind ───────────────────────────────────────────────────

    /// Snapshot the arena's current write position.  Pair with
    /// `Rewind` to drop every allocation made in between without
    /// resetting the whole arena.
    [[nodiscard]] Marker Mark() const noexcept;

    /// Rewind the arena to a previously captured marker.  Every
    /// block that was appended after the marker is retained (so the
    /// memory is still committed), but its offset is rolled back to
    /// zero; future allocations fall back into the original block
    /// first and only expand into the tail once it fills again.
    void Rewind(Marker marker) noexcept;

    /// Drop every allocation and every block offset back to zero.
    /// Blocks themselves are kept so the next tick re-uses the same
    /// pages without a heap round-trip.  Invalidates every marker.
    void Reset() noexcept;

    /// Release every block back to the heap and start over with a
    /// single fresh block.  Invalidates every marker.  Used when the
    /// arena has grown so large that shrinking it is worth the heap
    /// round-trip — mostly for tests that want a clean slate.
    void Shrink() noexcept;

    // ── Introspection ─────────────────────────────────────────────────────

    [[nodiscard]] const ArenaStats& Stats() const noexcept { return stats_; }
    [[nodiscard]] std::size_t       BlockBytes() const noexcept { return blockBytes_; }
    [[nodiscard]] std::size_t       BlockCount() const noexcept { return blocks_.size(); }

private:
    // ─── Internal block layout ───────────────────────────────────────────

    struct Block
    {
        std::unique_ptr<std::uint8_t[]> data;
        std::size_t                     size   = 0; ///< Capacity in bytes.
        std::size_t                     offset = 0; ///< Next free byte within this block.
    };

    /// Append a new block sized `max(blockBytes_, minBytes)`.  Used
    /// both when the head block runs out of room and when an
    /// oversized request needs a dedicated block.
    [[nodiscard]] bool AppendBlock(std::size_t minBytes) noexcept;

    /// Clamp an alignment request up to the next power of two.  The
    /// arena rejects zero alignments by forcing them to
    /// `alignof(std::max_align_t)`.
    [[nodiscard]] static std::size_t NormaliseAlignment(std::size_t alignment) noexcept;

    // ─── Members ─────────────────────────────────────────────────────────

    std::vector<Block>   blocks_;
    std::size_t          blockBytes_  = kDefaultBlockBytes; ///< Default block capacity.
    std::uint32_t        activeBlock_ = 0;                  ///< Index of the block the next allocation will try first.
    ArenaStats           stats_{};
};

// ─── ScopedArena ──────────────────────────────────────────────────────────

/// RAII helper that captures a marker in its constructor and rewinds
/// in its destructor.  Used to scope arena usage around a block of
/// work without having to remember a matching `Rewind` call:
///
///     {
///         ScopedArena scope(arena);
///         auto* scratch = arena.Allocate(4096);
///         DoWork(scratch);
///     } // arena rewinds here
///
/// The scope is noncopyable and nonmovable — the lifetime contract
/// is "exactly one rewind per marker".
class ScopedArena
{
public:
    explicit ScopedArena(ArenaAllocator& arena) noexcept
        : arena_(arena)
        , marker_(arena.Mark())
    {
    }

    ~ScopedArena() noexcept { arena_.Rewind(marker_); }

    ScopedArena(const ScopedArena&)            = delete;
    ScopedArena& operator=(const ScopedArena&) = delete;
    ScopedArena(ScopedArena&&)                 = delete;
    ScopedArena& operator=(ScopedArena&&)      = delete;

private:
    ArenaAllocator&         arena_;
    ArenaAllocator::Marker  marker_;
};

} // namespace SagaEngine::Core
