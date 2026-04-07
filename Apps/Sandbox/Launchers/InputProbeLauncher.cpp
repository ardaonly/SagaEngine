/// @file InputProbeLauncher.cpp
/// @brief Focused launcher: opens the sandbox with InputProbeScenario pre-selected.
///
/// This file is intentionally minimal. All logic lives in SandboxHost.
/// The launcher only constructs a config and calls Run().

#include <SagaSandbox/Core/SandboxHost.h>
#include <SagaSandbox/Core/SandboxConfig.h>
#include <SagaEngine/Core/Log/Log.h>

int main()
{
    SagaEngine::Core::Log::Init();

    SagaSandbox::SandboxConfig cfg;
    cfg.initialScenarioId = "input_probe";
    cfg.windowTitle       = "SagaEngine Sandbox — Input Probe";

    SagaSandbox::SandboxHost host{cfg};
    host.Run();

    SagaEngine::Core::Log::Shutdown();
    return 0;
}