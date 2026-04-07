/// @file MemoryDiagScenario.cpp

#include "MemoryDiagScenario.h"
#include "SagaSandbox/Core/ScenarioRegistry.h"
#include <SagaEngine/Core/Log/Log.h>
#include <SagaEngine/Core/Memory/MemoryTracker.h>
#include <imgui.h>
#include <chrono>

namespace SagaSandbox
{

SAGA_SANDBOX_REGISTER_SCENARIO(MemoryDiagScenario);

static constexpr const char* kTag = "MemoryDiagScenario";

bool MemoryDiagScenario::OnInit()
{
    LOG_INFO(kTag, "Initialising memory diag…");
    m_arena = std::make_unique<SagaEngine::Core::ArenaAllocator>(4 * 1024 * 1024);
    m_pool  = std::make_unique<SagaEngine::Core::PoolAllocator<SbDummyObject>>();
    return true;
}

void MemoryDiagScenario::OnUpdate(float /*dt*/, uint64_t /*tick*/) {}

void MemoryDiagScenario::OnShutdown()
{
    // Free intentional leaks on shutdown so we don't pollute subsequent runs
    for (void* p : m_intentionalLeaks)
        ::operator delete(p);
    m_intentionalLeaks.clear();

    m_pool.reset();
    m_arena.reset();
    LOG_INFO(kTag, "Memory diag shut down.");
}

// ─── Benchmarks ───────────────────────────────────────────────────────────────

void MemoryDiagScenario::RunArenaBenchmark(int iterations)
{
    m_arena->Reset();
    const auto t0 = std::chrono::steady_clock::now();

    for (int i = 0; i < iterations; ++i)
    {
        void* p = m_arena->Allocate(sizeof(SbDummyObject), alignof(SbDummyObject));
        (void)p;
    }

    const auto t1 = std::chrono::steady_clock::now();
    m_lastArenaMs = std::chrono::duration<float, std::milli>(t1 - t0).count();
}

void MemoryDiagScenario::RunPoolBenchmark(int iterations)
{
    // Allocate then immediately free
    const auto t0 = std::chrono::steady_clock::now();

    std::vector<SbDummyObject*> ptrs;
    ptrs.reserve(iterations);
    for (int i = 0; i < iterations; ++i)
        ptrs.push_back(m_pool->Allocate());
    for (auto* p : ptrs)
        m_pool->Deallocate(p);

    const auto t1 = std::chrono::steady_clock::now();
    m_lastPoolMs = std::chrono::duration<float, std::milli>(t1 - t0).count();
}

void MemoryDiagScenario::AllocateIntentionalLeak()
{
    // Allocate without freeing — MemoryTracker should catch this
    SagaEngine::Core::MemoryTracker::Instance().SetTag("SandboxIntentionalLeak");
    auto* p = new char[64];
    m_intentionalLeaks.push_back(p);
    ++m_leakCount;
    LOG_WARN(kTag, "Intentional leak #%d allocated.", m_leakCount);
}

// ─── Debug UI ─────────────────────────────────────────────────────────────────

void MemoryDiagScenario::OnRenderDebugUI()
{
    if (!ImGui::Begin("Memory Diagnostics"))
    {
        ImGui::End();
        return;
    }

    // ── Live stats ────────────────────────────────────────────────────────────

    ImGui::SeparatorText("MemoryTracker");

    const auto& mt = SagaEngine::Core::MemoryTracker::Instance();
    const size_t totalBytes = mt.GetTotalAllocated();
    const size_t allocCount = mt.GetAllocationCount();

    ImGui::Text("Total heap    : %.3f MB",
                static_cast<float>(totalBytes) / (1024.f * 1024.f));
    ImGui::Text("Allocations   : %zu", allocCount);

    if (ImGui::Button("DumpLeaks to Log"))
        SagaEngine::Core::MemoryTracker::Instance().DumpLeaks();

    // ── Intentional leak ──────────────────────────────────────────────────────

    ImGui::SeparatorText("Leak Testing");

    ImGui::Text("Leaked so far : %d", m_leakCount);
    if (ImGui::Button("Allocate intentional leak"))
        AllocateIntentionalLeak();

    ImGui::SameLine();
    if (ImGui::Button("Free all leaks"))
    {
        for (void* p : m_intentionalLeaks)
            delete[] static_cast<char*>(p);
        m_intentionalLeaks.clear();
        m_leakCount = 0;
    }

    // ── Arena benchmark ───────────────────────────────────────────────────────

    ImGui::SeparatorText("ArenaAllocator Benchmark");

    ImGui::SliderInt("Iterations", &m_benchIterations, 1000, 100000);

    if (ImGui::Button("Run Arena"))
        RunArenaBenchmark(m_benchIterations);

    ImGui::Text("Arena time   : %.3f ms  (%.2f ns/alloc)",
                m_lastArenaMs,
                m_benchIterations > 0
                    ? m_lastArenaMs * 1'000'000.f / static_cast<float>(m_benchIterations)
                    : 0.f);

    ImGui::Text("Arena blocks : %zu", m_arena->GetBlockCount());
    ImGui::Text("Arena total  : %.3f KB",
                static_cast<float>(m_arena->GetTotalAllocated()) / 1024.f);

    // ── Pool benchmark ────────────────────────────────────────────────────────

    ImGui::SeparatorText("PoolAllocator Benchmark");

    if (ImGui::Button("Run Pool"))
        RunPoolBenchmark(m_benchIterations);

    ImGui::Text("Pool time    : %.3f ms  (%.2f ns/op)",
                m_lastPoolMs,
                m_benchIterations > 0
                    ? m_lastPoolMs * 1'000'000.f / static_cast<float>(m_benchIterations)
                    : 0.f);

    ImGui::Text("Pool alloc'd : %zu", m_pool->GetAllocatedCount());
    ImGui::Text("Pool free    : %zu", m_pool->GetFreeCount());

    ImGui::End();
}

} // namespace SagaSandbox