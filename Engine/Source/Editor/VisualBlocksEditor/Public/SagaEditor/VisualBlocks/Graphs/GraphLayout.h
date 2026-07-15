#pragma once
namespace SagaEditor::VisualBlocks {
class GraphDocument;
class GraphLayout {
public:
    static void AutoLayout(GraphDocument& doc);
    static void Compact(GraphDocument& doc);
};
} // namespace SagaEditor::VisualBlocks
