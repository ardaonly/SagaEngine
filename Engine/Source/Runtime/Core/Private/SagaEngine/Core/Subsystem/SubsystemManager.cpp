#include "SagaEngine/Core/Subsystem/SubsystemManager.h"
#include <algorithm>

namespace SagaEngine::Core
{
void SubsystemManager::Register(std::unique_ptr<ISubsystem> system)
{
    m_systems.push_back(std::move(system));
}

bool SubsystemManager::InitAll()
{
    for (auto& s : m_systems) {
        if (!s->OnInit()) return false;
    }
    return true;
}

void SubsystemManager::UpdateAll(float dt)
{
    for (auto& s : m_systems) s->OnUpdate(dt);
}

void SubsystemManager::ShutdownAll()
{
    for (auto it = m_systems.rbegin(); it != m_systems.rend(); ++it) (*it)->OnShutdown();
}

ISubsystem* SubsystemManager::FindByName(const char* name)
{
    for (auto& s : m_systems) if (std::string(s->GetName()) == name) return s.get();
    return nullptr;
}
}