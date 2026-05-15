/// @file SelectionFilter.h
/// @brief Predicate-based filtering for editor selectable handles.

#pragma once

#include <cstdint>
#include <functional>
#include <vector>

namespace SagaEditor
{

// ─── Selectable Handle ────────────────────────────────────────────────────────

using SelectableId = uint64_t;

// ─── Selection Filter ─────────────────────────────────────────────────────────

/// Applies an optional predicate to ordered selectable id collections.
class SelectionFilter
{
public:
    using Predicate = std::function<bool(SelectableId)>;

    /// Replace the current predicate; an empty predicate accepts every id.
    void SetPredicate(Predicate pred);

    /// Return the ids accepted by the predicate while preserving input order.
    std::vector<SelectableId> Filter(const std::vector<SelectableId>& ids) const;

    /// Return true when the id is accepted by the current predicate.
    [[nodiscard]] bool Passes(SelectableId id) const;

private:
    Predicate m_pred; ///< Optional predicate used to gate selectable ids.
};

} // namespace SagaEditor
