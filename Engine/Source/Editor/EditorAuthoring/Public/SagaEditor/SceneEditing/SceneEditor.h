// SPDX-License-Identifier: MPL-2.0
#pragma once
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
namespace SagaEditor {
class SceneDocument;
class SceneEditor {
public:
    SceneEditor();
    ~SceneEditor();
    bool        OpenScene(const std::string& path);
    void        CloseScene();
    bool        SaveScene();
    bool        IsModified()  const noexcept;
    [[nodiscard]] SceneDocument* GetDocument() noexcept;
    [[nodiscard]] const SceneDocument* GetDocument() const noexcept;
    bool        RenameEntity(std::string_view entityId, std::string name);
    void        SetOnSceneChanged(std::function<void()> cb);
private:
    struct Impl; std::unique_ptr<Impl> m_impl;
};
} // namespace SagaEditor
