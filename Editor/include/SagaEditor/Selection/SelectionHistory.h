#pragma once
#include <cstdint>
#include <vector>
namespace SagaEditor {
using SelectableId = uint64_t;
class SelectionHistory {
public:
    void Push(const std::vector<SelectableId>& selection);
    bool CanGoBack()    const noexcept;
    bool CanGoForward() const noexcept;
    const std::vector<SelectableId>& GoBack();
    const std::vector<SelectableId>& GoForward();
    void Clear();
private:
    std::vector<std::vector<SelectableId>> m_history;
    size_t m_cursor = 0;
};
} // namespace SagaEditor
