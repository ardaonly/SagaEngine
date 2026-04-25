#pragma once
#include <functional>
#include <memory>
namespace SagaEditor::VisualScripting {
class GraphDocument;
class GraphPreviewRunner {
public:
    explicit GraphPreviewRunner(GraphDocument& doc);
    ~GraphPreviewRunner();
    void Run();
    void Stop();
    bool IsRunning() const noexcept;
    void SetOnStep(std::function<void()> cb);
private:
    struct Impl; std::unique_ptr<Impl> m_impl;
};
} // namespace SagaEditor::VisualScripting
