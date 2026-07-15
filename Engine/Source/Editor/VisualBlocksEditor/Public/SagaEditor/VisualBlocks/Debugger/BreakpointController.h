#pragma once
#include "SagaEditor/VisualBlocks/Graphs/GraphDocument.h"
#include <cstdint>
#include <unordered_set>
namespace SagaEditor::VisualBlocks {
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
} // namespace SagaEditor::VisualBlocks
