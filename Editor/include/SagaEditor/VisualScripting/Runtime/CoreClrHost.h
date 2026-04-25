#pragma once
#include <memory>
#include <string>
namespace SagaEditor::VisualScripting {
class CoreClrHost {
public:
    CoreClrHost();
    ~CoreClrHost();
    bool Load(const std::string& clrPath);
    void Unload();
    bool IsLoaded() const noexcept;
private:
    struct Impl; std::unique_ptr<Impl> m_impl;
};
} // namespace SagaEditor::VisualScripting
