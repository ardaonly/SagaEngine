/// @file SelectionFilter.cpp
/// @brief SelectionFilter implementation.

#include "SagaEditor/Selection/SelectionFilter.h"

#include <utility>

namespace SagaEditor
{

void SelectionFilter::SetPredicate(Predicate pred)
{
    m_pred = std::move(pred);
}

std::vector<SelectableId> SelectionFilter::Filter(
    const std::vector<SelectableId>& ids) const
{
    std::vector<SelectableId> filtered;
    filtered.reserve(ids.size());

    for (SelectableId id : ids)
    {
        if (Passes(id))
            filtered.push_back(id);
    }

    return filtered;
}

bool SelectionFilter::Passes(SelectableId id) const
{
    return !m_pred || m_pred(id);
}

} // namespace SagaEditor
