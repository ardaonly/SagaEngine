#pragma once
namespace SagaEditor::VisualScripting {
class GraphDocument;
class GraphLayout {
public:
    static void AutoLayout(GraphDocument& doc);
    static void Compact(GraphDocument& doc);
};
} // namespace SagaEditor::VisualScripting
