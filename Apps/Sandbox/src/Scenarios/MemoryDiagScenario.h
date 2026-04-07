/// @file MemoryDiagScenario.h
/// @brief Scenario for memory leak detection and allocator throughput.
///
/// Layer  : Sandbox / Scenarios
/// Purpose: Exercises MemoryTracker, ArenaAllocator, and PoolAllocator.
///          Can deliberately leak memory to verify DumpLeaks() output.

#pragma once

#include <SagaSandbox/Core/IScenario.h>
#include <SagaEngine/Core/Memory/ArenaAllocator.h>
#include <SagaEngine/Core/Memory/PoolAllocator.h>
#include <memory>
#include <vector>

namespace SagaSandbox
{

struct SbDummyObject { float data[16] = {}; };

class MemoryDiagScenario final : public IScenario
{
public:
    static constexpr ScenarioDescriptor kDescriptor
    {
        .id          = "memory_diag",
        .displayName = "Memory Diagnostics",
        .category    = "Memory",
        .description = "MemoryTracker leak detection, Arena/Pool allocator throughput.",
    };

    MemoryDiagScenario()  = default;
    ~MemoryDiagScenario() override = default;

    [[nodiscard]] const ScenarioDescriptor& GetDescriptor() const noexcept override
    { return kDescriptor; }

    [[nodiscard]] bool OnInit()                          override;
    void               OnUpdate(float dt, uint64_t tick) override;
    void               OnShutdown()                      override;
    void               OnRenderDebugUI()                 override;

private:
    void RunArenaBenchmark(int iterations);
    void RunPoolBenchmark(int iterations);
    void AllocateIntentionalLeak();

    std::unique_ptr<SagaEngine::Core::ArenaAllocator>              m_arena;
    std::unique_ptr<SagaEngine::Core::PoolAllocator<SbDummyObject>> m_pool;

    float    m_lastArenaMs   = 0.f;
    float    m_lastPoolMs    = 0.f;
    int      m_benchIterations = 10000;
    int      m_leakCount     = 0;

    // Pointers we deliberately don't free (to test DumpLeaks)
    std::vector<void*> m_intentionalLeaks;
};

} // namespace SagaSandbox