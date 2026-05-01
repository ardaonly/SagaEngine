/// @file BlockScript.h
/// @brief Top-level container for a single Scratch-style block program.

#pragma once

#include "SagaEditor/VisualScripting/Blocks/BlockInstance.h"
#include "SagaEditor/VisualScripting/Blocks/BlockStack.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace SagaEditor::VisualScripting
{

class BlockLibrary;

// ─── Block Script ─────────────────────────────────────────────────────────────

/// Owns every `BlockInstance` and every `BlockStack` in a single
/// block program. The top-level list (`rootStacks`) holds stacks
/// rooted at hat blocks plus the free-floating "shelf" of
/// in-progress blocks the user has not yet attached to a hat.
///
/// Mutation is intentionally simple: callers receive the strongly-
/// typed handles and pass them back to the script for every edit.
/// Validation is best-effort — the editor does not currently refuse
/// edits that violate snap rules; it surfaces them as warnings via
/// `Validate`.
class BlockScript
{
public:
    BlockScript();
    ~BlockScript();

    BlockScript(const BlockScript&)            = delete;
    BlockScript& operator=(const BlockScript&) = delete;
    BlockScript(BlockScript&&) noexcept;
    BlockScript& operator=(BlockScript&&) noexcept;

    // ─── Construction ─────────────────────────────────────────────────────────

    /// Allocate a fresh `BlockInstance` referencing `opcode` and
    /// return its handle. The instance is created detached — it does
    /// not belong to any stack until it is appended to one.
    BlockInstanceId CreateInstance(std::string opcode);

    /// Allocate a fresh empty `BlockStack` and return its handle.
    /// Optionally seed the stack with an initial head instance.
    BlockStackId CreateStack(BlockInstanceId head = {});

    /// Promote a stack to the script's root list. Used when the user
    /// drags a hat block onto an empty area of the canvas. Idempotent.
    void AddRootStack(BlockStackId stack);

    /// Remove a stack from the root list (for example because the
    /// user just nested it inside a C-block). Does not delete the
    /// stack itself; the stack still exists and can be re-rooted or
    /// reattached.
    void RemoveRootStack(BlockStackId stack) noexcept;

    /// Append `instance` to the bottom of `stack`. The instance must
    /// not already belong to a stack.
    bool AppendToStack(BlockStackId stack, BlockInstanceId instance);

    /// Set the slot fill for `instance`'s slot `slotId`. Returns
    /// false when the instance does not exist. Does not validate
    /// against the underlying definition's slot kind — the editor
    /// chooses to accept "wrong" fills and surface a validator
    /// warning instead, so users can fix problems at their own pace.
    bool SetSlotFill(BlockInstanceId instance,
                     const std::string& slotId,
                     BlockSlotFill fill);

    /// Attach `child` as the `mouthIndex`-th child stack of
    /// `parent`. Used by C and E blocks. Returns false when the
    /// parent does not exist or the index is out of range for the
    /// parent's mouth count.
    bool AttachMouth(BlockInstanceId parent,
                     std::uint8_t    mouthIndex,
                     BlockStackId    child);

    // ─── Lookup ───────────────────────────────────────────────────────────────

    [[nodiscard]] const BlockInstance* FindInstance(BlockInstanceId id) const noexcept;
    [[nodiscard]] BlockInstance*       FindInstance(BlockInstanceId id)       noexcept;
    [[nodiscard]] const BlockStack*    FindStack   (BlockStackId    id) const noexcept;
    [[nodiscard]] BlockStack*          FindStack   (BlockStackId    id)       noexcept;

    [[nodiscard]] std::size_t InstanceCount() const noexcept;
    [[nodiscard]] std::size_t StackCount()    const noexcept;

    /// Snapshot of the root-stack ids in registration order. Returned
    /// by value; iterators into the internal storage never escape.
    [[nodiscard]] std::vector<BlockStackId> GetRootStacks() const;

    // ─── Validation ───────────────────────────────────────────────────────────

    /// Issues raised by `Validate`. Each issue carries the offending
    /// id (instance or stack) and a human-readable message. The
    /// editor renders these as in-place warnings on the canvas.
    struct ValidationIssue
    {
        enum class Severity : std::uint8_t { Warning, Error };
        Severity    severity = Severity::Warning;
        std::string message;
        BlockInstanceId instance;
        BlockStackId    stack;
    };

    /// Walk the script and emit a list of issues against `library`.
    /// Validation is non-mutating — call sites can run it on every
    /// edit. The check covers:
    ///   * unknown opcodes,
    ///   * stack head must be Hat / Stack / Cap (no orphan
    ///     Reporter / Boolean at a stack head),
    ///   * Hat blocks may only be the head of a stack,
    ///   * Cap blocks may only be the tail of a stack,
    ///   * mouth count matches `BlockKindMouthCount(kind)`,
    ///   * slot fills referencing an `Expression` instance id resolve
    ///     to a Reporter or Boolean instance.
    [[nodiscard]] std::vector<ValidationIssue>
        Validate(const BlockLibrary& library) const;

private:
    // ─── Internal Storage ─────────────────────────────────────────────────────

    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace SagaEditor::VisualScripting
