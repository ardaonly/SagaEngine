#pragma once
#include <functional>
#include <string>
#include <unordered_map>
namespace SagaEditor::VisualScripting {
class WatchController {
public:
    void Watch(const std::string& expr);
    void Unwatch(const std::string& expr);
    std::string Evaluate(const std::string& expr) const;
    void SetOnValueChanged(std::function<void(const std::string& expr, const std::string& val)> cb);
private:
    std::unordered_map<std::string, std::string> m_watched;
    std::function<void(const std::string&, const std::string&)> m_cb;
};
} // namespace SagaEditor::VisualScripting
