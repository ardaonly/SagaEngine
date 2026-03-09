#include "SagaEngine/Simulation/SimulationTick.h"
#include "SagaEngine/Simulation/WorldState.h"
#include "SagaEngine/Core/Log/Log.h"
#include <algorithm>

namespace SagaEngine::Simulation
{

SimulationTick::SimulationTick()
    : m_Config{}
{
    LOG_INFO("SimulationTick", "SimulationTick initialized (60 Hz)");
}

SimulationTick::SimulationTick(TickConfig config)
    : m_Config(std::move(config))
{
    LOG_INFO("SimulationTick", "SimulationTick initialized (%.2f Hz)", 
             1.0f / m_Config.fixedTimestep);
}

SimulationTick::~SimulationTick()
{
    LOG_INFO("SimulationTick", "SimulationTick shutdown (total ticks: %llu)", m_TickCount);
}

void SimulationTick::SetWorldState(WorldState* world)
{
    std::lock_guard<std::mutex> lk(m_Mutex);
    m_World = world;
    
    for (auto& system : m_Systems) {
        system->SetWorldState(world);
    }
}

void SimulationTick::RegisterSystem(std::unique_ptr<ECS::ISystem> system)
{
    std::lock_guard<std::mutex> lk(m_Mutex);
    
    if (!system) {
        LOG_ERROR("SimulationTick", "Cannot register null system");
        return;
    }
    
    system->SetWorldState(m_World);
    m_Systems.push_back(std::move(system));
    
    std::sort(m_Systems.begin(), m_Systems.end(),
        [](const auto& a, const auto& b) {
            return a->GetPriority() > b->GetPriority();
        });
    
    LOG_INFO("SimulationTick", "Registered system: %s (priority: %d)", 
             system->GetName(), system->GetPriority());
}

void SimulationTick::Tick(float deltaTime)
{
    if (m_Paused) {
        return;
    }
    
    std::lock_guard<std::mutex> lk(m_Mutex);
    
    if (m_Config.catchUp) {
        m_AccumulatedTime += deltaTime;
        
        int subSteps = 0;
        while (m_AccumulatedTime >= m_Config.fixedTimestep && 
               subSteps < m_Config.maxSubSteps) {
            ExecuteSystems(m_Config.fixedTimestep);
            m_AccumulatedTime -= m_Config.fixedTimestep;
            m_TickCount++;
            subSteps++;
        }
        
        if (subSteps >= m_Config.maxSubSteps) {
            LOG_WARN("SimulationTick", "Max substeps reached (%d), skipping %.4f seconds", 
                     subSteps, m_AccumulatedTime);
            m_AccumulatedTime = 0.0f;
        }
    } else {
        ExecuteSystems(deltaTime);
        m_TickCount++;
    }
}

void SimulationTick::ExecuteSystems(float dt)
{
    for (auto& system : m_Systems) {
        system->OnSystemUpdate(dt);
    }
}

} // namespace SagaEngine::Simulation