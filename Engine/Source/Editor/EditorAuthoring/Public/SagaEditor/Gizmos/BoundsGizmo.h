#pragma once
#include <cstdint>
namespace SagaEditor {
class BoundsGizmo {
public:
    void SetTargetEntity(uint64_t entityId);
    void Draw();
private:
    uint64_t m_target = 0;
};
} // namespace SagaEditor
