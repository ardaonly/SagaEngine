#pragma once
#include <functional>
#include <string>
namespace SagaEditor {
class MainToolbar {
public:
    MainToolbar();
    ~MainToolbar();
    void AddButton(const std::string& id, const std::string& label,
                   std::function<void()> onClick);
    void RemoveButton(const std::string& id);
    void SetButtonEnabled(const std::string& id, bool enabled);
private:
    struct Impl; std::unique_ptr<Impl> m_impl;
};
} // namespace SagaEditor
