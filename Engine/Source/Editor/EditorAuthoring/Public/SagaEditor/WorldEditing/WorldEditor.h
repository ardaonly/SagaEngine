#pragma once
#include <functional>
#include <memory>
#include <string>
namespace SagaEditor {
class WorldEditor {
public:
    WorldEditor();
    ~WorldEditor();
    bool OpenWorld(const std::string& path);
    void CloseWorld();
    bool SaveWorld();
    bool IsModified() const noexcept;
    void SetOnWorldChanged(std::function<void()> cb);
private:
    struct Impl; std::unique_ptr<Impl> m_impl;
};
} // namespace SagaEditor
