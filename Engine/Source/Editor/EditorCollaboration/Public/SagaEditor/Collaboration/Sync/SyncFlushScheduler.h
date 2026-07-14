#pragma once
#include <cstdint>
#include <functional>
namespace SagaEditor::Collaboration {
class SyncFlushScheduler {
public:
    explicit SyncFlushScheduler(uint32_t intervalMs = 50);
    void Start(std::function<void()> flush);
    void Stop();
    void ForceFlush();
private:
    uint32_t m_intervalMs;
    std::function<void()> m_flush;
};
} // namespace SagaEditor::Collaboration
