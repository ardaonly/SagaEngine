#pragma once
#include "SagaEditor/VisualBlocks/Graphs/GraphDocument.h"
namespace SagaEditor::VisualBlocks {
class ExecutionOverlay {
public:
    void HighlightNode(NodeId id);
    void ClearHighlights();
    void Draw();
private:
    NodeId m_highlighted{0};
};
} // namespace SagaEditor::VisualBlocks
