#pragma once
#include <memory>
namespace SagaEditor::VisualBlocks {
class GraphDocument;
class EvaluationSession {
public:
    explicit EvaluationSession(GraphDocument& doc);
    ~EvaluationSession();
    bool Start();
    void Stop();
    bool IsRunning() const noexcept;
    void Tick(float dt);
private:
    struct Impl; std::unique_ptr<Impl> m_impl;
};
} // namespace SagaEditor::VisualBlocks
