#pragma once
#include <memory>
#include <string>
namespace SagaEditor::VisualScripting {
class ScriptHost {
public:
    ScriptHost();
    ~ScriptHost();
    bool Initialize(const std::string& runtimePath);
    void Shutdown();
    bool IsReady() const noexcept;
private:
    struct Impl; std::unique_ptr<Impl> m_impl;
};
} // namespace SagaEditor::VisualScripting
