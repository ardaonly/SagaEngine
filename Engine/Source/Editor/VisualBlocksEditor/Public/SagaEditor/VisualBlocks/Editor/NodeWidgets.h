#pragma once
#include "SagaEditor/VisualScripting/Graphs/GraphDocument.h"
namespace SagaEditor::VisualScripting {
class NodeWidgets {
public:
    static void DrawNode(const NodeData& node, bool selected);
    static bool HitTest(const NodeData& node, float mx, float my) noexcept;
};
} // namespace SagaEditor::VisualScripting
