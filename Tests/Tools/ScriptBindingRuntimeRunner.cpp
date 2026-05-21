/// @file ScriptBindingRuntimeRunner.cpp
/// @brief BUILD_TESTING-only editorless SagaScript binding runtime runner.

#include <SagaEngine/Scripting/CSharpScriptHost.hpp>
#include <SagaEngine/Scripting/ScriptBindingRuntime.hpp>
#include <SagaEngine/Scripting/WorldStateScriptWorld.hpp>
#include <SagaEngine/Simulation/WorldState.h>

#include <filesystem>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

namespace
{

using SagaEngine::Scripting::CSharpScriptHost;
using SagaEngine::Scripting::CSharpScriptHostOptions;
using SagaEngine::Scripting::ScriptBindingRuntime;
using SagaEngine::Scripting::ScriptBindingRuntimeCreateRequest;
using SagaEngine::Scripting::ScriptDiagnostic;
using SagaEngine::Scripting::ScriptHostOperationResult;
using SagaEngine::Scripting::ScriptLifecycleService;
using SagaEngine::Scripting::ScriptPackageValidationOptions;
using SagaEngine::Scripting::WorldStateScriptWorld;
using SagaEngine::Simulation::WorldState;

void PrintDiagnostics(const std::vector<ScriptDiagnostic>& diagnostics)
{
    for (const auto& diagnostic : diagnostics)
    {
        std::cerr << diagnostic.diagnostic.code.value << ": "
                  << diagnostic.diagnostic.message;
        if (!diagnostic.bindingId.empty())
        {
            std::cerr << " bindingId=" << diagnostic.bindingId;
        }
        if (!diagnostic.scriptId.empty())
        {
            std::cerr << " scriptId=" << diagnostic.scriptId;
        }
        for (const auto& [key, value] : diagnostic.metadata)
        {
            std::cerr << ' ' << key << '=' << value;
        }
        std::cerr << '\n';
    }
}

[[nodiscard]] bool Check(
    const ScriptHostOperationResult& result,
    std::string_view operation)
{
    if (result.Succeeded())
    {
        return true;
    }

    std::cerr << "ScriptBindingRuntimeRunner failed during " << operation << '\n';
    PrintDiagnostics(result.diagnostics);
    return false;
}

} // namespace

int main(int argc, char** argv)
{
    std::filesystem::path projectRoot;
    bool run = false;

    for (int index = 1; index < argc; ++index)
    {
        const std::string argument = argv[index];
        if (argument == "--project" && index + 1 < argc)
        {
            projectRoot = argv[++index];
            continue;
        }
        if (argument == "--run")
        {
            run = true;
            continue;
        }

        std::cerr << "Usage: ScriptBindingRuntimeRunner --project <project-root> --run\n";
        return 2;
    }

    if (projectRoot.empty() || !run)
    {
        std::cerr << "Usage: ScriptBindingRuntimeRunner --project <project-root> --run\n";
        return 2;
    }

    WorldState worldState;
    WorldStateScriptWorld scriptWorld(worldState);

    CSharpScriptHostOptions hostOptions;
#if defined(SAGA_SCRIPT_RUNTIME_BRIDGE_ASSEMBLY)
    hostOptions.runtimeBridgeAssembly =
        std::filesystem::path(SAGA_SCRIPT_RUNTIME_BRIDGE_ASSEMBLY);
#endif
#if defined(SAGA_DOTNET_ROOT)
    hostOptions.dotnetRoot = std::filesystem::path(SAGA_DOTNET_ROOT);
#endif
    hostOptions.world = &scriptWorld;

    CSharpScriptHost host(std::move(hostOptions));

    ScriptPackageValidationOptions validationOptions;
    validationOptions.expectedPackageDestination = "server";
    ScriptLifecycleService lifecycle(host, {}, validationOptions);

    ScriptBindingRuntime runtime(lifecycle, scriptWorld);
    ScriptBindingRuntimeCreateRequest request;
    request.packageRequest.packageRoot = projectRoot;
    request.packageRequest.scriptArtifactManifest =
        "Build/Manifests/script_artifacts.json";
    request.packageRequest.packageId = "script.binding.runtime.runner";
    request.manifestRequest.packageRoot = projectRoot;

    if (!Check(runtime.LoadAndCreate(request), "load/create"))
    {
        static_cast<void>(runtime.Shutdown());
        return 1;
    }
    if (!Check(runtime.StartAll(), "start"))
    {
        static_cast<void>(runtime.Shutdown());
        return 1;
    }
    if (!Check(runtime.UpdateAll(1.0 / 60.0), "update"))
    {
        static_cast<void>(runtime.Shutdown());
        return 1;
    }
    if (!Check(runtime.Shutdown(), "shutdown"))
    {
        return 1;
    }

    return 0;
}
