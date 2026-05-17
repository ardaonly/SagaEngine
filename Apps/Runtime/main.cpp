/// @file main.cpp
/// @brief Temporary SagaRuntime adapter over the current client host.
///
/// v0.0.8 ships a role-based runtime binary while the runtime core is still
/// being separated from the older client entrypoint. Keep editor/tooling
/// dependencies out of this adapter; Phase 2 should split Runtime Core from
/// optional client UI semantics.

#include "ClientHost.h"

#include <SagaEngine/Core/Log/Log.h>
#include <SagaEngine/Platform/PlatformFactory.h>
#include <SagaEngine/Startup/RuntimeStartupGate.hpp>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <optional>
#include <string>
#include <utility>

namespace
{

constexpr const char* kLogTag = "Runtime";

struct RuntimeLaunchConfig
{
    Saga::ClientConfig clientConfig;
    std::optional<std::filesystem::path> packageManifestPath;
};

void LogStartupDiagnostic(
    const SagaEngine::Startup::RuntimeStartupGateDiagnostic& diagnostic)
{
    const std::string manifestPath = diagnostic.manifestPath.string();
    const std::string resolvedPath =
        diagnostic.resolvedPath.has_value() ? diagnostic.resolvedPath->string() : "";

    LOG_ERROR(kLogTag,
              "Startup package validation failed: %s: %s (manifest='%s')",
              diagnostic.diagnosticId.c_str(),
              diagnostic.message.c_str(),
              manifestPath.c_str());
    if (!resolvedPath.empty())
    {
        LOG_ERROR(kLogTag, "Startup diagnostic resolved path: %s", resolvedPath.c_str());
    }
}

bool ValidateStartupPackage(const RuntimeLaunchConfig& launchConfig)
{
    if (!launchConfig.packageManifestPath.has_value())
    {
        LOG_INFO(kLogTag,
                 "No --package-manifest supplied; skipping startup package validation for dev compatibility.");
        return true;
    }

    SagaEngine::Startup::RuntimeStartupGateOptions options;
    options.packageManifestPath = *launchConfig.packageManifestPath;
    options.expectedDomain = SagaEngine::Startup::RuntimeStartupDomain::Client;

    const auto result =
        SagaEngine::Startup::RuntimeStartupGate::ValidatePackageForStartup(options);
    if (result.Succeeded())
    {
        LOG_INFO(kLogTag,
                 "Startup package validation accepted '%s'.",
                 launchConfig.packageManifestPath->string().c_str());
        return true;
    }

    for (const auto& diagnostic : result.diagnostics)
    {
        LogStartupDiagnostic(diagnostic);
    }

    return false;
}

} // namespace

// ─── Command-line parsing ─────────────────────────────────────────────────────

static RuntimeLaunchConfig ParseArgs(int argc, char* argv[])
{
    RuntimeLaunchConfig launchConfig;

    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];

        if ((arg == "--server" || arg == "-s") && i + 1 < argc)
        {
            launchConfig.clientConfig.serverHost = argv[++i];
        }
        else if (arg == "--port" && i + 1 < argc)
        {
            launchConfig.clientConfig.serverPort =
                static_cast<std::uint16_t>(std::stoi(argv[++i]));
        }
        else if (arg == "--headless" || arg == "-h")
        {
            launchConfig.clientConfig.headless = true;
        }
        else if (arg == "--package-manifest" && i + 1 < argc)
        {
            launchConfig.packageManifestPath = std::filesystem::path(argv[++i]);
        }
        else if (arg == "--help" || arg == "--?")
        {
            std::printf("Usage: SagaRuntime [options]\n"
                        "  --server, -s <host>   Server address (default: 127.0.0.1)\n"
                        "  --port <port>         Server port (default: 7777)\n"
                        "  --headless, -h        Run without window / renderer\n"
                        "  --package-manifest <path>\n"
                        "                         Validate startup package manifest before initialization\n"
                        "  --help, --?           Show this help\n");
            std::exit(0);
        }
    }

    return launchConfig;
}

// ─── Entry point ──────────────────────────────────────────────────────────────

int main(int argc, char* argv[])
{
    SagaEngine::Core::Log::Init();

    auto launchConfig = ParseArgs(argc, argv);
    if (!ValidateStartupPackage(launchConfig))
    {
        SagaEngine::Core::Log::Shutdown();
        return 1;
    }

    auto platformFactory = Saga::CreateSDLPlatformFactory();
    Saga::PlatformFactory::Set(platformFactory.get());

    Saga::ClientHost host(std::move(launchConfig.clientConfig));
    host.Run();

    SagaEngine::Core::Log::Shutdown();
    return 0;
}
