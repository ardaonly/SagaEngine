#pragma once
#include <vector>
#include <memory>
#include <string>
#include "ISubsystem.h"

namespace SagaEngine::Core
{
class SubsystemManager
{
public:
    void Register(std::unique_ptr<ISubsystem> system);
    bool InitAll();
    void UpdateAll(float dt);
    void ShutdownAll();
    ISubsystem* FindByName(const char* name);
private:
    std::vector<std::unique_ptr<ISubsystem>> m_systems;
};
}