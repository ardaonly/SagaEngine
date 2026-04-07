/// @file ECSProbeScenario.h
/// @brief Scenario that exercises the full ECS pipeline at runtime.
///
/// Layer  : Sandbox / Scenarios
/// Purpose: Spawns/destroys entities, adds/removes components, runs queries,
///          and measures throughput. Verifies archetype transitions and
///          serialisation round-trips.

#pragma once

#include "SagaSandbox/Core/IScenario.h"
#include <SagaEngine/Simulation/WorldState.h>
#include <SagaEngine/Simulation/SimulationTick.h>
#include <SagaEngine/ECS/Query.h>
#include <memory>
#include <vector>
#include <cstdint>

namespace SagaSandbox
{

// ─── Test Components (sandbox-private, not registered globally) ───────────────

struct SbTransform
{
    float x = 0.f, y = 0.f, z = 0.f;
};

struct SbVelocity
{
    float vx = 0.f, vy = 0.f, vz = 0.f;
};

struct SbHealth
{
    float current = 100.f;
    float max     = 100.f;
};

// ─── Scenario ─────────────────────────────────────────────────────────────────

class ECSProbeScenario final : public IScenario
{
public:
    static constexpr ScenarioDescriptor kDescriptor
    {
        .id          = "ecs_probe",
        .displayName = "ECS Probe",
        .category    = "ECS",
        .description = "Entity create/destroy, component add/remove, query throughput.",
    };

    ECSProbeScenario()  = default;
    ~ECSProbeScenario() override = default;

    [[nodiscard]] const ScenarioDescriptor& GetDescriptor() const noexcept override
    {
        return kDescriptor;
    }

    [[nodiscard]] bool OnInit()                          override;
    void               OnUpdate(float dt, uint64_t tick) override;
    void               OnShutdown()                      override;
    void               OnRenderDebugUI()                 override;

private:
    // ── Helpers ───────────────────────────────────────────────────────────────

    void SpawnBatch(int count);
    void DestroyRandom(int count);
    void RunQueryBenchmark();

    // ── State ─────────────────────────────────────────────────────────────────

    std::unique_ptr<SagaEngine::Simulation::WorldState>     m_world;
    std::unique_ptr<SagaEngine::Simulation::SimulationTick> m_simTick;

    std::vector<SagaEngine::ECS::EntityId> m_entities;

    // HUD sliders
    int   m_spawnCount   = 100;
    int   m_destroyCount = 10;
    bool  m_autoSpawn    = false;

    // Stats
    uint64_t m_totalSpawned   = 0;
    uint64_t m_totalDestroyed = 0;
    float    m_lastQueryMs    = 0.f;
    uint32_t m_lastQueryHits  = 0;
};

} // namespace SagaSandbox