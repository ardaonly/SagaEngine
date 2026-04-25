#pragma once
#include "SagaEditor/VisualScripting/Graphs/GraphDocument.h"
#include <unordered_set>
namespace SagaEditor::VisualScripting {
class BreakpointController {
public:
    void Toggle(NodeId node);
    void Set(NodeId node);
    void Clear(NodeId node);
    void ClearAll();
    bool Has(NodeId node) const noexcept;
private:
    std::unordered_set<uint64_t> m_breakpoints;
};
} // namespace SagaEditor::VisualScripting
