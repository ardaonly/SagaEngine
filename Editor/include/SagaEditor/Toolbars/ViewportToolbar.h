#pragma once
#include <functional>
#include <string>
namespace SagaEditor {
class ViewportToolbar {
public:
    void AddAction(const std::string& id, const std::string& label,
                   std::function<void()> onClick);
    void RemoveAction(const std::string& id);
    void SetActionEnabled(const std::string& id, bool enabled);
private:
    struct Impl; std::unique_ptr<Impl> m_impl;
};
} // namespace SagaEditor
