/// @file BlockSnapRules.h
/// @brief Kind-driven adjacency rules — what can connect to what.
///
/// The block canvas asks `BlockSnapRules` whether a candidate drop
/// target accepts the dragged block before highlighting the slot
/// green. Every check is pure: same inputs → same answer; no editor
/// state, no Qt, no graph-document mutation.

#pragma once

#include "SagaEditor/VisualScripting/Blocks/BlockDefinition.h"
#include "SagaEditor/VisualScripting/Blocks/BlockInstance.h"
#include "SagaEditor/VisualScripting/Blocks/BlockKind.h"
#include "SagaEditor/VisualScripting/Blocks/BlockSlot.h"

#include <cstdint>
#include <string>

namespace SagaEditor::VisualScripting
{

class BlockLibrary;
class BlockScript;

// ─── Drop-Target Categories ───────────────────────────────────────────────────

/// Where on the canvas the user is trying to drop a block. Each
/// category has its own acceptance predicate driven by `BlockKind`.
enum class DropTargetKind : std::uint8_t
{
    /// Free canvas — accepts a Hat (becomes the head of a new stack)
    /// or a Stack/C/E/Cap with no required predecessor (forms a shelf
    /// stack the user can later attach to a hat).
    EmptyCanvas,

    /// Above the head of an existing stack — accepts Stack / C / E / Cap.
    /// Hat is rejected because hats may only sit at the top, and the
    /// existing head displaces.
    StackTop,

    /// Below the tail of an existing stack — accepts Stack / C / E / Cap.
    /// Rejects Hat (hats are top-only) and Cap when the stack already
    /// ends in a Cap (caps are cap-once).
    StackBottom,

    /// Between two existing instances inside a stack — accepts Stack / C / E.
    /// Rejects Hat (top-only), Cap (must terminate a stack), and any
    /// expression block.
    StackBetween,

    /// A reporter / boolean slot on a block — accepts only the matching
    /// expression kind whose `BlockOutputType` is compatible.
    ExpressionSlot,

    /// The mouth of a C-block or E-block — accepts Stack / C / E / Cap.
    /// The first block dropped becomes the head of the mouth's child stack.
    Mouth,
};

[[nodiscard]] const char*    DropTargetKindId(DropTargetKind kind) noexcept;
[[nodiscard]] DropTargetKind DropTargetKindFromId(const std::string& id) noexcept;

// ─── Drop Target ──────────────────────────────────────────────────────────────

/// Descriptor passed into `CanSnap`. The canvas builds one of these
/// for the slot under the cursor; the function decides yes / no.
struct BlockDropTarget
{
    DropTargetKind  kind = DropTargetKind::EmptyCanvas;

    /// For StackTop / StackBottom / StackBetween / Mouth — the parent
    /// stack the drop targets. Ignored for EmptyCanvas / ExpressionSlot.
    BlockStackId    targetStack;

    /// For StackBetween — the index *before which* the new block lands
    /// (so 0 means "above the head", `size()` means "below the tail").
    /// Ignored for other kinds.
    std::size_t     insertIndex = 0;

    /// For ExpressionSlot / Mouth — the host instance.
    BlockInstanceId hostInstance;

    /// For ExpressionSlot — the slot id on `hostInstance`.
    std::string     slotId;

    /// For Mouth — the mouth index (0 for C-block, 0 or 1 for E-block).
    std::uint8_t    mouthIndex = 0;

    // ─── Convenience Builders ─────────────────────────────────────────────────

    [[nodiscard]] static BlockDropTarget MakeEmptyCanvas() noexcept;
    [[nodiscard]] static BlockDropTarget MakeStackTop   (BlockStackId stack) noexcept;
    [[nodiscard]] static BlockDropTarget MakeStackBottom(BlockStackId stack) noexcept;
    [[nodiscard]] static BlockDropTarget MakeStackBetween(BlockStackId stack,
                                                          std::size_t  index) noexcept;
    [[nodiscard]] static BlockDropTarget MakeExpressionSlot(BlockInstanceId host,
                                                             std::string slotId);
    [[nodiscard]] static BlockDropTarget MakeMouth(BlockInstanceId host,
                                                    std::uint8_t    mouthIndex) noexcept;
};

// ─── Snap Rejection Reason ────────────────────────────────────────────────────

/// Why a snap was rejected. Returned alongside the boolean so the
/// canvas can show a tooltip ("Hat blocks must start a stack",
/// "Reporter slot expects a Number, this returns Text", etc.).
enum class SnapRejection : std::uint8_t
{
    Accepted,
    UnknownOpcode,           ///< The dragged block's opcode is not in the library.
    HatMustStartStack,       ///< Hat dropped anywhere except StackTop / EmptyCanvas.
    HatNotAllowedHere,       ///< Hat dropped onto an existing-stack target.
    CapMustEndStack,         ///< Cap dropped between, in a mouth that already has a tail, etc.
    CapCannotPrecedeStack,   ///< Trying to drop a non-cap *below* a cap-terminated stack.
    ExpressionInStack,       ///< Reporter / Boolean dropped onto a stack target.
    StackBlockInExpression,  ///< Stack block dropped into an expression slot.
    SlotTypeMismatch,        ///< Expression dropped into a slot whose type doesn't match.
    SlotDoesNotAccept,       ///< Slot is dropdown / colour / statement — no expression allowed.
    MouthIndexOutOfRange,    ///< Mouth index ≥ the host's mouth count.
    HostHasNoSuchSlot,       ///< Slot id not present on the host's definition.
    InvalidStack,             ///< Target stack id does not exist in the script.
    InvalidHost,              ///< Target host instance id does not exist in the script.
    InsertIndexOutOfRange,   ///< StackBetween index > stack.size().
};

[[nodiscard]] const char* SnapRejectionId(SnapRejection r) noexcept;

// ─── Snap Verdict ─────────────────────────────────────────────────────────────

/// Result of a `CanSnap` query. `accepted` means the canvas should
/// highlight the slot green and let the drop happen; `rejection`
/// gives a machine-readable reason for the tooltip and a hover state.
struct SnapVerdict
{
    bool          accepted  = false;
    SnapRejection rejection = SnapRejection::Accepted;

    [[nodiscard]] bool operator==(const SnapVerdict& o) const noexcept
    {
        return accepted == o.accepted && rejection == o.rejection;
    }
};

// ─── Snap Predicates ──────────────────────────────────────────────────────────

/// Decide whether a block of opcode `draggedOpcode` may snap onto
/// `target` inside `script`. Looks the dragged opcode up in
/// `library` for its `BlockKind` and `BlockOutputType`. Pure
/// function: no script mutation, no logging side-effect.
[[nodiscard]] SnapVerdict CanSnap(const std::string&     draggedOpcode,
                                  const BlockDropTarget& target,
                                  const BlockScript&     script,
                                  const BlockLibrary&    library) noexcept;

/// Cheap precheck used by the palette renderer to grey out blocks
/// that cannot snap at the current hovered target. Same answer as
/// `CanSnap` but returns only the boolean.
[[nodiscard]] inline bool IsAcceptedSnap(const std::string&     draggedOpcode,
                                         const BlockDropTarget& target,
                                         const BlockScript&     script,
                                         const BlockLibrary&    library) noexcept
{
    return CanSnap(draggedOpcode, target, script, library).accepted;
}

} // namespace SagaEditor::VisualScripting
