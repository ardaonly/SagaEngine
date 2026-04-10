/// @file PartialComponentDirty.h
/// @brief Field-level dirty tracking for selective component replication.
///
/// Layer  : SagaEngine / ECS
/// Purpose: The default dirty signal is one bit per component instance —
///          "this component changed, resend all of it".  For big components
///          like `TransformComponent` (position + rotation + velocity +
///          bounds) that wastes 60–80% of the bandwidth because only one
///          field usually changes per tick.  Partial dirty tracking stores
///          a per-field bitmask so the replication layer can send just the
///          changed fields.
///
/// Design:
///   - `PartialDirtyMask` is a thin 64-bit wrapper — components with up to
///     64 replicated fields get their dirty state in a single cache-friendly
///     word.  We deliberately cap at 64 because a single component with
///     more than 64 replicated fields is almost certainly a design smell
///     and should be split.
///   - Fields are identified by a compile-time `FieldIndex` (0..63).
///     Each component declares its own enum so field names don't collide.
///   - Merging two masks (OR) is used when two writers mutate the same
///     component between sync points; only one combined mask is sent.
///   - Clearing is the replication layer's responsibility — the component
///     author never calls `Clear()` directly, only `Mark*()`.
///
/// Why a bitmask and not a version counter per field:
///   Version counters (a la Netcode's "dirty ticks") cost 4 bytes per
///   field vs 1 bit, and they require the receiver to also store previous
///   tick state.  For MMO traffic volumes the bitmask is 32–100× smaller
///   and the replication layer already stashes a prior snapshot for
///   delta encoding.

#pragma once

#include <cstdint>
#include <type_traits>

namespace SagaEngine::ECS {

// ─── PartialDirtyMask ───────────────────────────────────────────────────────

/// 64-bit field-granularity dirty mask.  Default-constructed to "nothing
/// dirty" — `IsClean()` returns true on a fresh instance.
///
/// Thread-safety: this type is NOT thread-safe on its own.  Component
/// writes already happen inside the ECS's system scheduler, which
/// guarantees exclusive write access during a tick, so we avoid the cost
/// of atomic ops.  If you need to set bits from multiple threads, wrap
/// with `std::atomic<std::uint64_t>` at the call site.
struct PartialDirtyMask
{
    std::uint64_t bits = 0;

    // ── Mutators ───────────────────────────────────────────────────────────

    /// Mark a single field as changed.  `field` is 0..63.
    constexpr void Mark(unsigned fieldIndex) noexcept
    {
        // Guard clause instead of an assert keeps release builds honest.
        // A >= 64 index is a programmer error, but silently clipping is
        // safer than UB if someone adds a 65th field and forgets to
        // regenerate the enum — the replication layer will just resend
        // the whole component, which is inefficient but correct.
        if (fieldIndex < 64u)
            bits |= (std::uint64_t{1} << fieldIndex);
    }

    /// Mark every bit at once — used when a system really does touch
    /// everything (e.g. teleport, respawn).  Equivalent to "resend all".
    constexpr void MarkAll() noexcept
    {
        bits = ~std::uint64_t{0};
    }

    /// Merge two masks.  Right-hand bits win; neither side is cleared.
    /// Used when the replication layer coalesces multiple ticks' worth
    /// of dirty state before a batched send.
    constexpr PartialDirtyMask& operator|=(const PartialDirtyMask& other) noexcept
    {
        bits |= other.bits;
        return *this;
    }

    /// Clear the mask.  Called by replication *after* it has captured
    /// the dirty fields into an outgoing snapshot.
    constexpr void Clear() noexcept
    {
        bits = 0;
    }

    // ── Queries ────────────────────────────────────────────────────────────

    /// Is this specific field dirty?
    [[nodiscard]] constexpr bool IsDirty(unsigned fieldIndex) const noexcept
    {
        return fieldIndex < 64u && ((bits >> fieldIndex) & std::uint64_t{1}) != 0;
    }

    /// Are *any* fields dirty?  Hot-path check in the replication loop:
    /// if this returns false we skip the component entirely.
    [[nodiscard]] constexpr bool IsDirtyAny() const noexcept
    {
        return bits != 0;
    }

    /// True iff nothing is marked.  Inverse of `IsDirtyAny()`, named for
    /// readability at call sites.
    [[nodiscard]] constexpr bool IsClean() const noexcept
    {
        return bits == 0;
    }

    /// Popcount — how many fields changed.  Used by the LOD/priority
    /// system to weight updates (many fields changed = higher priority).
    [[nodiscard]] constexpr unsigned DirtyCount() const noexcept
    {
        // Manual popcount to stay constexpr-friendly on C++17 toolchains
        // that don't yet have std::popcount.
        std::uint64_t b = bits;
        unsigned      n = 0;
        while (b != 0) { b &= (b - 1); ++n; }
        return n;
    }
};

// ─── Helper for declaring a field enum ──────────────────────────────────────

/// Use this in every component that supports partial replication:
///
/// @code
///   struct TransformComponent
///   {
///       enum Field : unsigned
///       {
///           kPosition = 0,
///           kRotation = 1,
///           kVelocity = 2,
///           kBounds   = 3,
///           kFieldCount
///       };
///       SAGA_STATIC_FIELD_LIMIT(TransformComponent);
///
///       SagaEngine::Math::Vec3 position;
///       SagaEngine::Math::Quat rotation;
///       SagaEngine::Math::Vec3 velocity;
///       SagaEngine::Math::Vec3 bounds;
///       SagaEngine::ECS::PartialDirtyMask dirty;
///   };
/// @endcode
///
/// The macro exists purely to catch the case where a field is added past
/// the 64-bit mask at compile time — otherwise the programmer only finds
/// out at runtime when replication drops bits silently.
#define SAGA_STATIC_FIELD_LIMIT(ComponentType)                                    \
    static_assert(static_cast<unsigned>(ComponentType::kFieldCount) <= 64,        \
                  #ComponentType                                                  \
                  ": PartialDirtyMask holds at most 64 fields. Split the "        \
                  "component or upgrade the mask to a multi-word bitset.")

static_assert(sizeof(PartialDirtyMask) == 8,
              "PartialDirtyMask must stay cache-line-friendly (8 bytes)");
static_assert(std::is_trivially_copyable_v<PartialDirtyMask>,
              "PartialDirtyMask must be trivially copyable for snapshot memcpy");

} // namespace SagaEngine::ECS
