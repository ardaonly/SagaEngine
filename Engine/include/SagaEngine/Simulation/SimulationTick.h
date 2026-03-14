#pragma once
#include "WorldState.h"
#include "SagaEngine/ECS/System.h"
#include <vector>
#include <memory>
#include <mutex>

namespace SagaEngine::Simulation
{

struct TickConfig
{
    float fixedTimestep{1.0f / 60.0f};
    int maxSubSteps{5};
    bool catchUp{true};
};

class SimulationTick
{
public:
    SimulationTick();
    explicit SimulationTick(TickConfig config);
    ~SimulationTick();
    
    void SetWorldState(WorldState* world);
    void RegisterSystem(std::unique_ptr<ECS::ISystem> system);
    
    void Tick(float deltaTime);
    
    float GetAccumulatedTime() const { return m_AccumulatedTime; }
    float GetFixedTimestep() const { return m_Config.fixedTimestep; }
    uint64_t GetTickCount() const { return m_TickCount; }
    
    void SetPaused(bool paused) { m_Paused = paused; }
    bool IsPaused() const { return m_Paused; }
    
private:
    TickConfig m_Config;
    WorldState* m_World{nullptr};
    std::vector<std::unique_ptr<ECS::ISystem>> m_Systems;
    std::mutex m_Mutex;
    
    float m_AccumulatedTime{0.0f};
    uint64_t m_TickCount{0};
    bool m_Paused{false};
    
    void ExecuteSystems(float dt);
};

} // namespace SagaEngine::Simulation