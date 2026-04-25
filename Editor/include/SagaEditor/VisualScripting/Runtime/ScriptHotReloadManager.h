#pragma once
#include <functional>
#include <memory>
#include <string>
namespace SagaEditor::VisualScripting {
class ScriptHotReloadManager {
public:
    ScriptHotReloadManager();
    ~ScriptHotReloadManager();
    void Watch(const std::string& assemblyPath);
    void Unwatch(const std::string& assemblyPath);
    void SetOnReloaded(std::function<void(const std::string& path)> cb);
private:
    struct Impl; std::unique_ptr<Impl> m_impl;
};
} // namespace SagaEditor::VisualScripting
