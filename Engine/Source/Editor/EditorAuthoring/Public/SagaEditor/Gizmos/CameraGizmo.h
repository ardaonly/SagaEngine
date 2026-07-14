#pragma once
#include <cstdint>
namespace SagaEditor {
class CameraGizmo {
public:
    void SetCameraEntity(uint64_t entityId);
    void Draw();
private:
    uint64_t m_camera = 0;
};
} // namespace SagaEditor
