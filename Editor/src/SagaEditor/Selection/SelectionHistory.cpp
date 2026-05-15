/// @file SelectionHistory.cpp
/// @brief SelectionHistory implementation.

#include "SagaEditor/Selection/SelectionHistory.h"

namespace SagaEditor
{

namespace
{

[[nodiscard]] const std::vector<SelectableId>& EmptySelection()
{
    static const std::vector<SelectableId> empty;
    return empty;
}

} // namespace

void SelectionHistory::Push(const std::vector<SelectableId>& selection)
{
    if (!m_history.empty() && m_history[m_cursor] == selection)
        return;

    if (!m_history.empty() && m_cursor + 1 < m_history.size())
    {
        m_history.erase(
            m_history.begin() + static_cast<std::ptrdiff_t>(m_cursor + 1),
            m_history.end());
    }

    m_history.push_back(selection);
    m_cursor = m_history.size() - 1;
}

bool SelectionHistory::CanGoBack() const noexcept
{
    return !m_history.empty() && m_cursor > 0;
}

bool SelectionHistory::CanGoForward() const noexcept
{
    return !m_history.empty() && m_cursor + 1 < m_history.size();
}

const std::vector<SelectableId>& SelectionHistory::GoBack()
{
    if (CanGoBack())
        --m_cursor;

    return m_history.empty() ? EmptySelection() : m_history[m_cursor];
}

const std::vector<SelectableId>& SelectionHistory::GoForward()
{
    if (CanGoForward())
        ++m_cursor;

    return m_history.empty() ? EmptySelection() : m_history[m_cursor];
}

void SelectionHistory::Clear()
{
    m_history.clear();
    m_cursor = 0;
}

} // namespace SagaEditor
