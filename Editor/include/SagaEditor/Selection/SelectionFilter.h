#pragma once
#include <cstdint>
#include <functional>
#include <vector>
namespace SagaEditor {
using SelectableId = uint64_t;
class SelectionFilter {
public:
    using Predicate = std::function<bool(SelectableId)>;
    void           SetPredicate(Predicate pred);
    std::vector<SelectableId> Filter(const std::vector<SelectableId>& ids) const;
    bool           Passes(SelectableId id) const;
private:
    Predicate m_pred;
};
} // namespace SagaEditor
