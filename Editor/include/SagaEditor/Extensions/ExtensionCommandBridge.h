#pragma once
#include <functional>
#include <string>
namespace SagaEditor {
class IExtensionCommand;
class ExtensionCommandBridge {
public:
    void Register(IExtensionCommand* cmd);
    void Unregister(const std::string& commandId);
    IExtensionCommand* Find(const std::string& commandId) const noexcept;
private:
    struct Impl; std::unique_ptr<Impl> m_impl;
};
} // namespace SagaEditor
