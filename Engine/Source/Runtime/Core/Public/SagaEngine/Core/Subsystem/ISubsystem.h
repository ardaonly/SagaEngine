#pragma once
#include <string>

namespace SagaEngine::Core
{
class ISubsystem
{
public:
    virtual ~ISubsystem() = default;
    virtual const char* GetName() const = 0;
    virtual bool OnInit() = 0;
    virtual void OnUpdate(float dt) = 0;
    virtual void OnShutdown() = 0;
};
}