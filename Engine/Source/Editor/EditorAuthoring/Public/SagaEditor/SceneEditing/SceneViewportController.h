#pragma once
#include <cstdint>
#include <functional>
namespace SagaEditor {
enum class ViewportCameraMode : uint8_t { Perspective, Orthographic };
class SceneViewportController {
public:
    void SetCameraMode(ViewportCameraMode mode);
    ViewportCameraMode GetCameraMode() const noexcept;
    void FocusOnEntity(uint64_t entityId);
    void ResetCamera();
    void SetOnCameraMoved(std::function<void()> cb);
private:
    ViewportCameraMode m_mode = ViewportCameraMode::Perspective;
    std::function<void()> m_cb;
};
} // namespace SagaEditor
