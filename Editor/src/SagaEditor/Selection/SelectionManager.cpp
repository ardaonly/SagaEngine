/// @file SelectionManager.cpp
/// @brief Tracks the current editor selection and notifies observers on change.

#include "SagaEditor/Selection/SelectionManager.h"
#include <algorithm>

namespace SagaEditor
{

// ─── Construction ─────────────────────────────────────────────────────────────

SelectionManager::SelectionManager(QObject* parent)
    : QObject(parent)
{}

// ─── Mutation ─────────────────────────────────────────────────────────────────

void SelectionManager::SetSelection(SelectableId id)
{
    m_selection = { id };
    emit SelectionChanged(m_selection);
}

void SelectionManager::SetSelection(std::vector<SelectableId> ids)
{
    m_selection = std::move(ids);
    emit SelectionChanged(m_selection);
}

void SelectionManager::AddToSelection(SelectableId id)
{
    if (IsSelected(id))
        return;

    m_selection.push_back(id);
    emit SelectionChanged(m_selection);
}

void SelectionManager::RemoveFromSelection(SelectableId id)
{
    auto it = std::find(m_selection.begin(), m_selection.end(), id);
    if (it == m_selection.end())
        return;

    m_selection.erase(it);
    emit SelectionChanged(m_selection);
}

void SelectionManager::ToggleSelection(SelectableId id)
{
    IsSelected(id) ? RemoveFromSelection(id) : AddToSelection(id);
}

void SelectionManager::ClearSelection()
{
    if (m_selection.empty())
        return;

    m_selection.clear();
    emit SelectionChanged(m_selection);
}

// ─── Queries ──────────────────────────────────────────────────────────────────

bool SelectionManager::IsSelected(SelectableId id) const noexcept
{
    return std::find(m_selection.begin(), m_selection.end(), id) != m_selection.end();
}

bool SelectionManager::IsEmpty()  const noexcept { return m_selection.empty();    }
std::size_t SelectionManager::Count() const noexcept { return m_selection.size(); }

SelectableId SelectionManager::GetPrimary() const noexcept
{
    return m_selection.empty() ? kInvalidSelectableId : m_selection.front();
}

const std::vector<SelectableId>& SelectionManager::GetAll() const noexcept
{
    return m_selection;
}

} // namespace SagaEditor
