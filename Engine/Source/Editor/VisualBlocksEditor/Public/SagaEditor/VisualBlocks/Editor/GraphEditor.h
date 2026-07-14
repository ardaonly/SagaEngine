#pragma once
#include <memory>
namespace SagaEditor::VisualScripting {
class GraphDocument;
class GraphEditor {
public:
    explicit GraphEditor(GraphDocument& doc);
    ~GraphEditor();
    void Open();
    void Close();
    bool IsOpen() const noexcept;
private:
    struct Impl; std::unique_ptr<Impl> m_impl;
};
} // namespace SagaEditor::VisualScripting
