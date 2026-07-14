#pragma once
#include <memory>
#include <string>
#include <vector>
namespace SagaEditor {
class IExtensionPanel;
class ExtensionPanelBridge {
public:
    
    ExtensionPanelBridge();
    ~ExtensionPanelBridge();
void Register(IExtensionPanel* panel);
    void Unregister(const std::string& panelId);
    std::vector<IExtensionPanel*> GetAll() const;
private:
    struct Impl; std::unique_ptr<Impl> m_impl;
};
} // namespace SagaEditor
