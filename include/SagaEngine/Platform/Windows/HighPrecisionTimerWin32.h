#pragma once

#include "SagaEngine/Platform/IHighPrecisionTimer.h"
#include <chrono>

namespace SagaEngine::Platform
{

class HighPrecisionTimerWin32 final : public IHighPrecisionTimer
{
public:
    HighPrecisionTimerWin32();
    ~HighPrecisionTimerWin32() override;
    void Reset() override;
    double GetTimeSeconds() const override;
    double GetDeltaSeconds() override;

private:
    bool qpcAvailable;
    long long frequency;
    long long startCount;
    long long lastCount;
    std::chrono::steady_clock::time_point steadyStart;
    std::chrono::steady_clock::time_point steadyLast;
    long long GetQPC() const noexcept;
};

} // namespace SagaEngine::Platform