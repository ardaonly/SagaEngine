#pragma once
#include <functional>
#include <string>
namespace SagaEditor::VisualScripting {
class ManagedToNativeBridge {
public:
    using Handler = std::function<std::string(const std::string& argsJson)>;
    void Register(const std::string& name, Handler handler);
    std::string Invoke(const std::string& name, const std::string& argsJson);
private:
    struct Impl; std::unique_ptr<Impl> m_impl;
};
} // namespace SagaEditor::VisualScripting
