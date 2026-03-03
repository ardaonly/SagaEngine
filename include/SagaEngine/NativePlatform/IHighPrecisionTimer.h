// include/SagaEngine/NativePlatform/IHighPrecisionTimer.h
#pragma once

namespace SagaEngine::NativePlatform
{
class IHighPrecisionTimer
{
public:
    virtual ~IHighPrecisionTimer() = default;

    virtual void Reset() = 0;

    virtual double GetTimeSeconds() const = 0;

    virtual double GetDeltaSeconds() = 0;
};
}
