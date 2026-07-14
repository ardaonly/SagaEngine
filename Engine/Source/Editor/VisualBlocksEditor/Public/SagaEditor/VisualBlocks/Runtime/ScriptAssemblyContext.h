#pragma once
#include <memory>
#include <string>
namespace SagaEditor::VisualScripting {
class ScriptAssemblyContext {
public:
    ScriptAssemblyContext();
    ~ScriptAssemblyContext();
    bool LoadAssembly(const std::string& path);
    void Unload();
    bool IsLoaded() const noexcept;
private:
    struct Impl; std::unique_ptr<Impl> m_impl;
};
} // namespace SagaEditor::VisualScripting
