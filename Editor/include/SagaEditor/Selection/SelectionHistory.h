/// @file SelectionHistory.h
/// @brief Linear undo-style history for editor selection snapshots.

#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace SagaEditor
{

// ─── Selectable Handle ────────────────────────────────────────────────────────

using SelectableId = uint64_t;

// ─── Selection History ────────────────────────────────────────────────────────

/// Stores ordered selection snapshots with back/forward navigation.
class SelectionHistory
{
public:
    /// Push a new current selection and discard any forward branch.
    void Push(const std::vector<SelectableId>& selection);

    [[nodiscard]] bool CanGoBack() const noexcept;
    [[nodiscard]] bool CanGoForward() const noexcept;

    /// Move to the previous selection if one exists and return the current state.
    const std::vector<SelectableId>& GoBack();

    /// Move to the next selection if one exists and return the current state.
    const std::vector<SelectableId>& GoForward();

    /// Remove all stored selections.
    void Clear();

private:
    std::vector<std::vector<SelectableId>> m_history; ///< Ordered selection snapshots.
    std::size_t m_cursor = 0;                         ///< Index of the current snapshot.
};

} // namespace SagaEditor
