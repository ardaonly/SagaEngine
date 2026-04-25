#pragma once
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
namespace SagaEditor {
class SceneEditor {
public:
    SceneEditor();
    ~SceneEditor();
    bool        OpenScene(const std::string& path);
    void        CloseScene();
    bool        SaveScene();
    bool        IsModified()  const noexcept;
    uint64_t    CreateEntity(const std::string& name = "Entity");
    void        DestroyEntity(uint64_t entityId);
    void        SetOnSceneChanged(std::function<void()> cb);
private:
    struct Impl; std::unique_ptr<Impl> m_impl;
};
} // namespace SagaEditor
