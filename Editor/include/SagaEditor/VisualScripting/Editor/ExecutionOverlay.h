#pragma once
#include "SagaEditor/VisualScripting/Graphs/GraphDocument.h"
namespace SagaEditor::VisualScripting {
class ExecutionOverlay {
public:
    void HighlightNode(NodeId id);
    void ClearHighlights();
    void Draw();
private:
    NodeId m_highlighted{0};
};
} // namespace SagaEditor::VisualScripting
