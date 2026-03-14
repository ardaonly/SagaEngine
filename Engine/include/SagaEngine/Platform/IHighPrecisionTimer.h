#pragma once

namespace SagaEngine::Platform
{

class IHighPrecisionTimer
{
public:
    virtual ~IHighPrecisionTimer() = default;
    virtual void Reset() = 0;
    virtual double GetTimeSeconds() const = 0;
    virtual double GetDeltaSeconds() = 0;
};

} // namespace SagaEngine::Platform