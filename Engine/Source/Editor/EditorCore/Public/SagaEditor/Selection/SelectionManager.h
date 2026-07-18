/// @file SelectionManager.h
/// @brief Tracks the current editor selection and notifies observers on change.

#pragma once

#include <cstdint>
#include <functional>
#include <utility>
#include <vector>

namespace SagaEditor
{

// ─── Selectable Handle ────────────────────────────────────────────────────────

/// Opaque handle identifying a selectable object in the editor.
/// ECS EntityId, asset path hash, or any stable 64-bit id maps here.
using SelectableId = uint64_t;
using SelectionSubscription = uint64_t;

/// Sentinel value for "nothing selected".
inline constexpr SelectableId kInvalidSelectableId = 0;
inline constexpr SelectionSubscription kInvalidSelectionSubscription = 0;

// ─── Selection Manager ────────────────────────────────────────────────────────

/// Single source of truth for what is selected. Panels read from here;
/// actions write through here so undo/redo can snapshot the selection delta.
/// Observers register callbacks that are invoked when selection changes.
class SelectionManager
{
public:
    /// Callback signature: invoked with the new selection set.
    using SelectionCallback = std::function<void(const std::vector<SelectableId>&)>;

    SelectionManager() = default;
    ~SelectionManager() = default;

    // ─── Mutation ─────────────────────────────────────────────────────────────

    /// Replace the selection with a single object.
    void SetSelection(SelectableId id);

    /// Replace the selection with a set of objects.
    void SetSelection(std::vector<SelectableId> ids);

    /// Add an object to the current selection.
    void AddToSelection(SelectableId id);

    /// Remove an object from the current selection.
    void RemoveFromSelection(SelectableId id);

    /// Toggle an object in/out of the selection.
    void ToggleSelection(SelectableId id);

    /// Clear the selection entirely.
    void ClearSelection();

    // ─── Queries ──────────────────────────────────────────────────────────────

    [[nodiscard]] bool IsSelected(SelectableId id) const noexcept;
    [[nodiscard]] bool IsEmpty()                   const noexcept;
    [[nodiscard]] std::size_t Count()              const noexcept;

    /// Return the primary (first) selected id, or kInvalidSelectableId.
    [[nodiscard]] SelectableId GetPrimary() const noexcept;

    /// Return all selected ids in selection order.
    [[nodiscard]] const std::vector<SelectableId>& GetAll() const noexcept;

    // ─── Observer Registration ────────────────────────────────────────────────

    /// Register a callback to be invoked on selection change.
    [[nodiscard]] SelectionSubscription Subscribe(SelectionCallback callback);
    void Unsubscribe(SelectionSubscription subscription);

private:
    std::vector<SelectableId> m_selection;      ///< Ordered selection list; front = primary.
    std::vector<std::pair<SelectionSubscription, SelectionCallback>> m_callbacks;
    SelectionSubscription m_nextSubscription = 1;

    /// Notify all subscribers of a change.
    void NotifyObservers();
};

} // namespace SagaEditor
