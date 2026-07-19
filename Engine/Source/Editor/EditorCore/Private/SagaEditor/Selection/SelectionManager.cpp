// SPDX-License-Identifier: MPL-2.0
/// @file SelectionManager.cpp
/// @brief Tracks the current editor selection and notifies observers on change.
///
/// Framework-free by design: the previous revision used `QObject` plus Qt
/// `emit SelectionChanged(...)` signals, which leaked Qt outside the
/// permitted backend folder. The header was already Qt-free and
/// declared a `std::function`-based `Subscribe` API; this file now
/// matches that contract — every observer is a registered
/// `SelectionCallback` and `NotifyObservers` walks the list directly.

#include "SagaEditor/Selection/SelectionManager.h"

#include <algorithm>
#include <exception>
#include <utility>

namespace SagaEditor
{

// ─── Mutation ─────────────────────────────────────────────────────────────────

void SelectionManager::SetSelection(SelectableId id)
{
    m_selection = { id };
    NotifyObservers();
}

void SelectionManager::SetSelection(std::vector<SelectableId> ids)
{
    m_selection = std::move(ids);
    NotifyObservers();
}

void SelectionManager::AddToSelection(SelectableId id)
{
    if (IsSelected(id))
    {
        return;
    }
    m_selection.push_back(id);
    NotifyObservers();
}

void SelectionManager::RemoveFromSelection(SelectableId id)
{
    auto it = std::find(m_selection.begin(), m_selection.end(), id);
    if (it == m_selection.end())
    {
        return;
    }
    m_selection.erase(it);
    NotifyObservers();
}

void SelectionManager::ToggleSelection(SelectableId id)
{
    if (IsSelected(id))
    {
        RemoveFromSelection(id);
    }
    else
    {
        AddToSelection(id);
    }
}

void SelectionManager::ClearSelection()
{
    if (m_selection.empty())
    {
        return;
    }
    m_selection.clear();
    NotifyObservers();
}

// ─── Queries ──────────────────────────────────────────────────────────────────

bool SelectionManager::IsSelected(SelectableId id) const noexcept
{
    return std::find(m_selection.begin(), m_selection.end(), id) != m_selection.end();
}

bool SelectionManager::IsEmpty() const noexcept
{
    return m_selection.empty();
}

std::size_t SelectionManager::Count() const noexcept
{
    return m_selection.size();
}

SelectableId SelectionManager::GetPrimary() const noexcept
{
    return m_selection.empty() ? kInvalidSelectableId : m_selection.front();
}

const std::vector<SelectableId>& SelectionManager::GetAll() const noexcept
{
    return m_selection;
}

// ─── Observer Registration ────────────────────────────────────────────────────

SelectionSubscription SelectionManager::Subscribe(SelectionCallback callback)
{
    if (!callback) return kInvalidSelectionSubscription;
    const auto subscription = m_nextSubscription++;
    m_callbacks.emplace_back(subscription, std::move(callback));
    return subscription;
}

void SelectionManager::Unsubscribe(SelectionSubscription subscription)
{
    std::erase_if(m_callbacks, [subscription](const auto& item)
    { return item.first == subscription; });
}

void SelectionManager::NotifyObservers()
{
    // Snapshot the list so a callback that re-enters Subscribe cannot
    // invalidate the iterator we are walking. A throwing callback must
    // not break the rest of the dispatch.
    const auto snapshot = m_callbacks;
    for (const auto& [subscription, cb] : snapshot)
    {
        (void)subscription;
        if (!cb) continue;
        try
        {
            cb(m_selection);
        }
        catch (...)
        {
            // Swallow — diagnostics belong in the editor log layer when wired.
        }
    }
}

} // namespace SagaEditor
