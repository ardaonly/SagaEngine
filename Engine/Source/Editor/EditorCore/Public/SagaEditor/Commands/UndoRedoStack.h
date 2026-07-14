/// @file UndoRedoStack.h
/// @brief Transactional undo/redo stack for all reversible editor actions.

#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace SagaEditor
{

// ─── Editor Action ────────────────────────────────────────────────────────────

/// One reversible editor action. Extensions and panels push IEditorAction
/// instances onto the undo stack rather than performing operations directly.
class IEditorAction
{
public:
    virtual ~IEditorAction() = default;

    /// Apply or re-apply this action.
    virtual void Execute() = 0;

    /// Reverse this action exactly.
    virtual void Undo() = 0;

    /// Human-readable label shown in the Undo/Redo menu items.
    [[nodiscard]] virtual std::string GetLabel() const = 0;
};

// ─── Undo/Redo Stack ─────────────────────────────────────────────────────────

/// Manages a linear undo history of IEditorAction instances.
/// Push always truncates the redo tail — standard linear undo semantics.
class UndoRedoStack
{
public:
    explicit UndoRedoStack(std::size_t maxDepth = 200);

    /// Execute an action and push it onto the history. Truncates redo tail.
    void Push(std::unique_ptr<IEditorAction> action);

    /// Undo the most recent action. No-op if the stack is empty.
    void Undo();

    /// Redo the next undone action. No-op if there is nothing to redo.
    void Redo();

    /// Remove all history entries.
    void Clear();

    [[nodiscard]] bool CanUndo() const noexcept;
    [[nodiscard]] bool CanRedo() const noexcept;

    /// Label of the action that would be undone next, or empty string.
    [[nodiscard]] std::string GetUndoLabel() const;

    /// Label of the action that would be redone next, or empty string.
    [[nodiscard]] std::string GetRedoLabel() const;

private:
    std::vector<std::unique_ptr<IEditorAction>> m_history;  ///< Ordered action log.
    std::size_t                                 m_cursor   = 0; ///< Points past the last executed action.
    std::size_t                                 m_maxDepth;     ///< Hard cap on history length.
};

} // namespace SagaEditor
