#pragma once
#include <cstdint>
#include <memory>
namespace SagaEditor::VisualScripting {
class PreviewWorld {
public:
    PreviewWorld();
    ~PreviewWorld();
    void Tick(float dt);
    void Reset();
private:
    struct Impl; std::unique_ptr<Impl> m_impl;
};
} // namespace SagaEditor::VisualScripting
