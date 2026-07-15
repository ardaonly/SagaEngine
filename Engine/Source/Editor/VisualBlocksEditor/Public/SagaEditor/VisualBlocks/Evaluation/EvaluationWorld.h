#pragma once
#include <cstdint>
#include <memory>
namespace SagaEditor::VisualBlocks {
class EvaluationWorld {
public:
    EvaluationWorld();
    ~EvaluationWorld();
    void Tick(float dt);
    void Reset();
private:
    struct Impl; std::unique_ptr<Impl> m_impl;
};
} // namespace SagaEditor::VisualBlocks
