/// @file ArenaAllocator.cpp
/// @brief Chained-block arena implementation with marker / rewind.

#include "SagaEngine/Core/Memory/ArenaAllocator.h"

#include <algorithm>
#include <cstring>

namespace SagaEngine::Core {

namespace {

/// Smallest alignment the arena will honour.  Zero or one-byte
/// alignments would defeat the alignment-pad math below, so we
/// silently promote to max-align.
constexpr std::size_t kMinAlignment = alignof(std::max_align_t);

/// Round `offset` up to the next multiple of `alignment`.  Requires
/// `alignment` to already be a power of two — callers go through
/// `NormaliseAlignment` first.
[[nodiscard]] std::size_t AlignUp(std::size_t offset,
                                  std::size_t alignment) noexcept
{
    return (offset + alignment - 1) & ~(alignment - 1);
}

} // namespace

// ─── Construction ─────────────────────────────────────────────────────────

ArenaAllocator::ArenaAllocator(std::size_t blockBytes) noexcept
    : blockBytes_(blockBytes == 0 ? kDefaultBlockBytes : blockBytes)
{
    // Prime the arena with one initial block so the first
    // `Allocate` is as cheap as every subsequent one.  Failure to
    // allocate the initial block leaves the arena empty; the next
    // `Allocate` will try again.
    AppendBlock(blockBytes_);
    stats_.bytesCommitted = blocks_.empty() ? 0 : blocks_.back().size;
    stats_.blockCount     = static_cast<std::uint32_t>(blocks_.size());
}

ArenaAllocator::~ArenaAllocator() noexcept = default;

// ─── Allocate ─────────────────────────────────────────────────────────────

void* ArenaAllocator::Allocate(std::size_t bytes,
                               std::size_t alignment) noexcept
{
    if (bytes == 0)
        return nullptr;

    const std::size_t alignedReq = NormaliseAlignment(alignment);

    // Try the active block first.  In the common case (no prior
    // Rewind) there is only one block and this path fires every
    // time.  If the block is full we append a fresh one and retry
    // exactly once — a second failure is a hard out-of-budget.
    for (std::size_t attempt = 0; attempt < 2; ++attempt)
    {
        if (activeBlock_ < blocks_.size())
        {
            Block& block = blocks_[activeBlock_];

            const std::size_t oldOffset = block.offset;
            const std::size_t aligned   = AlignUp(oldOffset, alignedReq);

            if (aligned + bytes <= block.size)
            {
                // Commit the allocation — update the block cursor
                // and the stats in lockstep so every counter stays
                // consistent even under allocation failure paths.
                std::uint8_t* base  = block.data.get() + aligned;
                block.offset        = aligned + bytes;

                const std::size_t padding = aligned - oldOffset;
                const std::size_t taken   = padding + bytes;

                stats_.bytesInUse     += taken;
                stats_.bytesAllocated += taken;
                ++stats_.allocations;

                return base;
            }
        }

        // Head block cannot satisfy the request.  Append a fresh
        // block large enough for the payload *and* worst-case
        // alignment padding so the retry always succeeds.
        if (!AppendBlock(bytes + alignedReq))
        {
            ++stats_.allocationFailures;
            return nullptr;
        }

        activeBlock_ = static_cast<std::uint32_t>(blocks_.size() - 1);
    }

    // Unreachable: either the second attempt succeeded and returned
    // above, or AppendBlock failed and returned nullptr already.
    ++stats_.allocationFailures;
    return nullptr;
}

// ─── Mark / Rewind ────────────────────────────────────────────────────────

ArenaAllocator::Marker ArenaAllocator::Mark() const noexcept
{
    Marker marker;
    marker.blockIndex  = activeBlock_;
    marker.blockOffset = (activeBlock_ < blocks_.size())
                             ? blocks_[activeBlock_].offset
                             : 0;
    return marker;
}

void ArenaAllocator::Rewind(Marker marker) noexcept
{
    if (marker.blockIndex >= blocks_.size())
    {
        // Marker is stale (e.g. arena was Shrink()ed in between).
        // Fall back to a full reset rather than silently misbehaving.
        Reset();
        return;
    }

    // Reset every block after the marker's block.  Their storage
    // stays allocated — future allocations will reuse the pages.
    for (std::size_t i = marker.blockIndex + 1; i < blocks_.size(); ++i)
    {
        blocks_[i].offset = 0;
    }

    // Trim the marker block's cursor back to where the marker was.
    blocks_[marker.blockIndex].offset = marker.blockOffset;

    // Active block becomes the marker block — the next `Allocate`
    // will retry there before touching any later blocks.
    activeBlock_ = marker.blockIndex;

    // Stats: `bytesInUse` is the sum of every block's current offset
    // so the rewind can simply recompute it from scratch.  The
    // allocation counters keep their high-water values.
    std::size_t inUse = 0;
    for (const auto& block : blocks_)
        inUse += block.offset;
    stats_.bytesInUse = inUse;
}

// ─── Reset / Shrink ───────────────────────────────────────────────────────

void ArenaAllocator::Reset() noexcept
{
    for (auto& block : blocks_)
        block.offset = 0;
    activeBlock_         = 0;
    stats_.bytesInUse    = 0;
    stats_.allocations   = 0;
    stats_.allocationFailures = 0;
    // `bytesAllocated` and `bytesCommitted` are high-water gauges —
    // they survive Reset so the operator can see worst-case usage
    // across the lifetime of the arena.
}

void ArenaAllocator::Shrink() noexcept
{
    blocks_.clear();
    activeBlock_ = 0;

    stats_.bytesInUse      = 0;
    stats_.bytesCommitted  = 0;
    stats_.blockCount      = 0;
    stats_.allocations     = 0;
    stats_.allocationFailures = 0;

    AppendBlock(blockBytes_);
    stats_.bytesCommitted = blocks_.empty() ? 0 : blocks_.back().size;
    stats_.blockCount     = static_cast<std::uint32_t>(blocks_.size());
}

// ─── AppendBlock ──────────────────────────────────────────────────────────

bool ArenaAllocator::AppendBlock(std::size_t minBytes) noexcept
{
    const std::size_t size = std::max(blockBytes_, minBytes);

    Block block;
    try
    {
        // `std::make_unique<T[]>` zero-initialises so the arena is
        // safe to read immediately after allocation, which keeps
        // fuzz tests deterministic and debuggers happy.
        block.data = std::make_unique<std::uint8_t[]>(size);
    }
    catch (...)
    {
        // Heap exhaustion — signal failure to the caller instead of
        // propagating, so game code stays noexcept.
        return false;
    }
    block.size   = size;
    block.offset = 0;

    blocks_.push_back(std::move(block));

    stats_.bytesCommitted += size;
    stats_.blockCount      = static_cast<std::uint32_t>(blocks_.size());
    return true;
}

// ─── NormaliseAlignment ───────────────────────────────────────────────────

std::size_t ArenaAllocator::NormaliseAlignment(std::size_t alignment) noexcept
{
    if (alignment < kMinAlignment)
        return kMinAlignment;

    // Promote non-powers-of-two up to the next power of two so the
    // `AlignUp` bit-mask stays valid.  Hot path is the fast bail-out.
    if ((alignment & (alignment - 1)) == 0)
        return alignment;

    std::size_t pow = kMinAlignment;
    while (pow < alignment)
        pow <<= 1;
    return pow;
}

} // namespace SagaEngine::Core
