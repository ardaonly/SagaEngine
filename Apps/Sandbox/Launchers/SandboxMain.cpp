/// @file SandboxMain.cpp
/// @brief Default sandbox launcher: opens with no pre-selected scenario.
///        The user picks one from the HUD scenario picker.
///
/// Command-line override:
///   --scenario <id>   Pre-select a scenario by ID.

#include <SagaSandbox/Core/SandboxHost.h>
#include <SagaSandbox/Core/SandboxConfig.h>
#include <SagaEngine/Core/Log/Log.h>
#include <cstring>

int main(int argc, char** argv)
{
    SagaEngine::Core::Log::Init();

    SagaSandbox::SandboxConfig cfg;

    // ── CLI argument parsing ──────────────────────────────────────────────────
    for (int i = 1; i < argc - 1; ++i)
    {
        if (std::strcmp(argv[i], "--scenario") == 0)
        {
            cfg.initialScenarioId = argv[i + 1];
        }
        else if (std::strcmp(argv[i], "--headless") == 0)
        {
            cfg.headless = true;
        }
    }

    SagaSandbox::SandboxHost host{cfg};
    host.Run();

    SagaEngine::Core::Log::Shutdown();
    return 0;
}