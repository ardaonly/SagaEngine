#pragma once
#include <cstdint>
#include <functional>
namespace SagaEditor::Collaboration {
class IOperationLog;
class ReplayController {
public:
    explicit ReplayController(IOperationLog& log);
    void Play(uint64_t fromSequenceId = 0);
    void Pause();
    void Stop();
    void SetSpeed(float speed);
    void SetOnStep(std::function<void(uint64_t sequenceId)> cb);
private:
    IOperationLog& m_log;
    float m_speed = 1.f;
    std::function<void(uint64_t)> m_onStep;
};
} // namespace SagaEditor::Collaboration
