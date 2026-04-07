/// @file Authority.cpp
/// @brief Authority method implementations.

#include "SagaEngine/Simulation/Authority.h"
#include "SagaEngine/Core/Log/Log.h"
#include "SagaEngine/Core/Profiling/Profiler.h"

namespace SagaEngine::Simulation {

void Authority::RegisterSystem(std::unique_ptr<ISimulationSystem> system)
{
    if (!system)
    {
        LOG_WARN("Authority", "RegisterSystem: null system pointer — ignored.");
        return;
    }

    LOG_INFO("Authority", "Registered system '%s' (index %u).",
             system->Name(), static_cast<uint32_t>(m_systems.size()));

    m_systems.push_back(std::move(system));
}

AuthorityTickResult Authority::Tick(
    uint64_t tick,
    double dt,
    std::span<const Input::ClientTickEntry> entries)
{
    SagaEngine::Core::Profiler::Instance().BeginSample("Authority::Tick");

    for (const auto& system : m_systems)
    {
        const char* systemName = system->Name();
        SagaEngine::Core::Profiler::Instance().BeginSample(systemName);
        system->Update(m_world, entries, tick, dt);
        SagaEngine::Core::Profiler::Instance().EndSample(systemName);
    }

    SagaEngine::Core::Profiler::Instance().EndSample("Authority::Tick");

    const uint64_t hash = m_deterministic.Record(tick, m_world);

    LOG_DEBUG("Authority",
        "Tick %llu complete — hash=0x%016llX entities=%u components=%u.",
        static_cast<unsigned long long>(tick),
        static_cast<unsigned long long>(hash),
        m_world.EntityCount(),
        m_world.TotalComponents());

    return AuthorityTickResult{
        .tick      = tick,
        .stateHash = hash,
        .diverged  = false,
    };
}

} // namespace SagaEngine::Simulation