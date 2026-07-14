/// @file UndoRedoStack.cpp
/// @brief Transactional undo/redo stack for all reversible editor actions.

#include "SagaEditor/Commands/UndoRedoStack.h"

namespace SagaEditor
{

// ─── Construction ─────────────────────────────────────────────────────────────

UndoRedoStack::UndoRedoStack(std::size_t maxDepth)
    : m_maxDepth(maxDepth)
{}

// ─── Push ─────────────────────────────────────────────────────────────────────

void UndoRedoStack::Push(std::unique_ptr<IEditorAction> action)
{
    // Truncate any undone actions above the cursor
    m_history.erase(m_history.begin() + static_cast<std::ptrdiff_t>(m_cursor),
                    m_history.end());

    // Evict oldest entry when the cap is reached
    if (m_history.size() >= m_maxDepth)
    {
        m_history.erase(m_history.begin());
        if (m_cursor > 0)
            --m_cursor;
    }

    action->Execute();
    m_history.push_back(std::move(action));
    ++m_cursor;
}

// ─── Undo / Redo ──────────────────────────────────────────────────────────────

void UndoRedoStack::Undo()
{
    if (!CanUndo())
        return;

    --m_cursor;
    m_history[m_cursor]->Undo();
}

void UndoRedoStack::Redo()
{
    if (!CanRedo())
        return;

    m_history[m_cursor]->Execute();
    ++m_cursor;
}

void UndoRedoStack::Clear()
{
    m_history.clear();
    m_cursor = 0;
}

// ─── Queries ──────────────────────────────────────────────────────────────────

bool UndoRedoStack::CanUndo() const noexcept { return m_cursor > 0; }
bool UndoRedoStack::CanRedo() const noexcept { return m_cursor < m_history.size(); }

std::string UndoRedoStack::GetUndoLabel() const
{
    return CanUndo() ? m_history[m_cursor - 1]->GetLabel() : std::string{};
}

std::string UndoRedoStack::GetRedoLabel() const
{
    return CanRedo() ? m_history[m_cursor]->GetLabel() : std::string{};
}

} // namespace SagaEditor
