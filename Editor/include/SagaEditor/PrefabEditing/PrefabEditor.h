#pragma once
#include <cstdint>
#include <memory>
#include <string>
namespace SagaEditor {
class PrefabEditor {
public:
    PrefabEditor();
    ~PrefabEditor();
    bool OpenPrefab(const std::string& prefabPath);
    void ClosePrefab();
    bool IsEditing() const noexcept;
    void ApplyChanges();
    void RevertChanges();
private:
    struct Impl; std::unique_ptr<Impl> m_impl;
};
} // namespace SagaEditor
