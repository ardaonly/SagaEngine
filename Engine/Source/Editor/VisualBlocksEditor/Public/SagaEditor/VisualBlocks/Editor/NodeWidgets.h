#pragma once
#include "SagaEditor/VisualBlocks/Graphs/GraphDocument.h"
namespace SagaEditor::VisualBlocks {
class NodeWidgets {
public:
    static void DrawNode(const NodeData& node, bool selected);
    static bool HitTest(const NodeData& node, float mx, float my) noexcept;
};
} // namespace SagaEditor::VisualBlocks
