#pragma once
#include <cstdint>
namespace SagaEditor {
enum class GizmoMode : uint8_t { Translate, Rotate, Scale, Universal };
class TransformGizmo {
public:
    void     SetMode(GizmoMode mode);
    GizmoMode GetMode() const noexcept;
    void     SetTargetEntity(uint64_t entityId);
    void     Draw();   ///< Called by viewport each frame; no-op until target set
    bool     IsActive() const noexcept;
private:
    GizmoMode m_mode    = GizmoMode::Translate;
    uint64_t  m_target  = 0;
    bool      m_active  = false;
};
} // namespace SagaEditor
