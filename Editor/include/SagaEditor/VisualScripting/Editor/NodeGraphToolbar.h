#pragma once
#include <functional>
#include <string>
namespace SagaEditor::VisualScripting {
class NodeGraphToolbar {
public:
    void AddAction(const std::string& id, const std::string& label,
                   std::function<void()> onClick);
    void RemoveAction(const std::string& id);
private:
    struct Impl; std::unique_ptr<Impl> m_impl;
};
} // namespace SagaEditor::VisualScripting
