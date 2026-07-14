#pragma once
#include <cstdint>
#include <functional>
namespace SagaEditor::Collaboration {
class LockHeartbeat {
public:
    explicit LockHeartbeat(uint32_t intervalMs = 5000);
    void Start(std::function<void()> tick);
    void Stop();
private:
    uint32_t m_intervalMs;
};
} // namespace SagaEditor::Collaboration
