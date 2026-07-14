/// @file HeadlessLauncher.cpp
/// @brief Headless sandbox launcher — no window, no ImGui.
///        Runs server-side scenarios (NetworkReplication, PredictionStress)
///        from CI or command line without any display dependency.
///
/// Usage:
///   SagaSandboxHeadless --scenario network_replication --ticks 1000

#include <SagaSandbox/Core/SandboxHost.h>
#include <SagaSandbox/Core/SandboxConfig.h>
#include <SagaEngine/Core/Log/Log.h>
#include <cstring>
#include <cstdlib>

int main(int argc, char** argv)
{
    SagaEngine::Core::Log::Init();

    SagaSandbox::SandboxConfig cfg;
    cfg.headless           = true;
    cfg.showHudOnStart     = false;
    cfg.captureMemoryStats = false;   // no HUD to show them
    cfg.verboseScenarioLog = true;

    // ── Parse CLI ─────────────────────────────────────────────────────────────
    for (int i = 1; i < argc - 1; ++i)
    {
        if (std::strcmp(argv[i], "--scenario") == 0)
            cfg.initialScenarioId = argv[i + 1];
    }

    if (cfg.initialScenarioId.empty())
    {
        // Default headless scenario
        cfg.initialScenarioId = "network_replication";
    }

    SagaSandbox::SandboxHost host{cfg};
    host.Run();

    SagaEngine::Core::Log::Shutdown();
    return 0;
}