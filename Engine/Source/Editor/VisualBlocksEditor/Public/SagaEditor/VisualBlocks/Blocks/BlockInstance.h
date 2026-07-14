/// @file BlockInstance.h
/// @brief Placed block instance — the runtime tree node of a `BlockScript`.

#pragma once

#include "SagaEditor/VisualScripting/Blocks/BlockSlot.h"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace SagaEditor::VisualScripting
{

// ─── Identity ─────────────────────────────────────────────────────────────────

/// Strongly-typed instance id. Wraps a 64-bit opaque value so the
/// type system catches stack / instance / slot-fill confusions at
/// compile time. `kBlockInstanceIdNone` is reserved and means "no
/// instance"; the script id allocator never hands it out to a real
/// instance.
struct BlockInstanceId
{
    std::uint64_t value = kBlockInstanceIdNone;

    [[nodiscard]] constexpr bool IsValid() const noexcept
    {
        return value != kBlockInstanceIdNone;
    }
    [[nodiscard]] constexpr bool operator==(const BlockInstanceId& o) const noexcept
    {
        return value == o.value;
    }
    [[nodiscard]] constexpr bool operator!=(const BlockInstanceId& o) const noexcept
    {
        return value != o.value;
    }
};

/// Strongly-typed stack id. Same rationale as `BlockInstanceId`.
struct BlockStackId
{
    std::uint64_t value = 0;

    [[nodiscard]] constexpr bool IsValid() const noexcept { return value != 0; }
    [[nodiscard]] constexpr bool operator==(const BlockStackId& o) const noexcept
    {
        return value == o.value;
    }
    [[nodiscard]] constexpr bool operator!=(const BlockStackId& o) const noexcept
    {
        return value != o.value;
    }
};

// ─── Block Instance ───────────────────────────────────────────────────────────

/// A single placed block. The `BlockScript` owns an
/// `unordered_map<id, BlockInstance>` and references each instance
/// from at most one place — either as a member of a `BlockStack`, as
/// a mouth's child stack head, or as the expression rooted in a
/// slot fill. The slot-fills map carries the per-slot value; the
/// `mouthStacks` vector holds child-stack ids for C / E blocks.
struct BlockInstance
{
    BlockInstanceId            id;          ///< Stable id within the script.
    std::string                opcode;      ///< Refers to a `BlockDefinition::opcode`.

    // Slot fills keyed by slot id (matches `BlockSlotDefinition::id`).
    std::unordered_map<std::string, BlockSlotFill> slotFills;

    // Mouth child stacks. Index `m` matches `mouthLabels[m]` on the
    // definition. C-blocks have one entry; E-blocks have two.
    std::vector<BlockStackId>  mouthStacks;

    // Optional comment attached by the user.
    std::string                comment;

    // Editor-only canvas position (used when the instance is the
    // top of a stack or the head of a "shelf" expression).
    float                      canvasX = 0.0f;
    float                      canvasY = 0.0f;

    [[nodiscard]] bool operator==(const BlockInstance& o) const noexcept;
    [[nodiscard]] bool operator!=(const BlockInstance& o) const noexcept
    {
        return !(*this == o);
    }
};

} // namespace SagaEditor::VisualScripting
