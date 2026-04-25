#pragma once
#include <cstdint>
#include <functional>
namespace SagaEditor::Collaboration {
class SessionHeartbeat {
public:
    explicit SessionHeartbeat(uint32_t intervalMs = 10000);
    void Start(std::function<void()> ping);
    void Stop();
    void RecordPong();
    bool IsAlive() const noexcept;
private:
    uint32_t m_intervalMs;
    bool m_alive = false;
};
} // namespace SagaEditor::Collaboration
