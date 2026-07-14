#pragma once
#include <memory>
#include <functional>
#include <string>
namespace SagaEditor {
class GraphToolbar {
public:
    
    GraphToolbar();
    ~GraphToolbar();
void AddAction(const std::string& id, const std::string& label,
                   std::function<void()> onClick);
    void RemoveAction(const std::string& id);
    void SetActionEnabled(const std::string& id, bool enabled);
private:
    struct Impl; std::unique_ptr<Impl> m_impl;
};
} // namespace SagaEditor
