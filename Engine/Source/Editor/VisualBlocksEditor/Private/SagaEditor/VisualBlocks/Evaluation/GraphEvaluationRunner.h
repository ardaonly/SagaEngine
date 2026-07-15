#pragma once
#include <functional>
#include <memory>
namespace SagaEditor::VisualBlocks {
class GraphDocument;
class GraphEvaluationRunner {
public:
    explicit GraphEvaluationRunner(GraphDocument& doc);
    ~GraphEvaluationRunner();
    void Run();
    void Stop();
    bool IsRunning() const noexcept;
    void SetOnStep(std::function<void()> cb);
private:
    struct Impl; std::unique_ptr<Impl> m_impl;
};
} // namespace SagaEditor::VisualBlocks
