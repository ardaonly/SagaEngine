#pragma once
#include <cstdint>
namespace SagaEditor {
class SplineGizmo {
public:
    void SetSplineEntity(uint64_t entityId);
    void Draw();
private:
    uint64_t m_spline = 0;
};
} // namespace SagaEditor
