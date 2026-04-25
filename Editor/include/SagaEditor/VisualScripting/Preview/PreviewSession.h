#pragma once
#include <memory>
namespace SagaEditor::VisualScripting {
class GraphDocument;
class PreviewSession {
public:
    explicit PreviewSession(GraphDocument& doc);
    ~PreviewSession();
    bool Start();
    void Stop();
    bool IsRunning() const noexcept;
    void Tick(float dt);
private:
    struct Impl; std::unique_ptr<Impl> m_impl;
};
} // namespace SagaEditor::VisualScripting
