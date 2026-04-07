/// @file NetworkProbeLauncher.cpp
/// @brief Focused launcher for network/replication scenarios.

#include <SagaSandbox/Core/SandboxHost.h>
#include <SagaSandbox/Core/SandboxConfig.h>
#include <SagaEngine/Core/Log/Log.h>

int main()
{
    SagaEngine::Core::Log::Init();

    SagaSandbox::SandboxConfig cfg;
    cfg.initialScenarioId = "network_replication";
    cfg.windowTitle       = "SagaEngine Sandbox — Network Replication";

    SagaSandbox::SandboxHost host{cfg};
    host.Run();

    SagaEngine::Core::Log::Shutdown();
    return 0;
}