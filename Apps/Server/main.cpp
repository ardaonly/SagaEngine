/// @file main.cpp
/// @brief Entry point for the SagaEngine test snapshot server.
///
/// This server:
///   - Listens on UDP for client connections
///   - Sends WorldState snapshots to connected clients
///   - Receives InputCommand packets and sends InputAck
///   - Logs all activity for verification
///
/// Usage: SagaServer [--port 7777] [--entities 5] [--snap-interval 0.5]

#include "TestSnapshotServer.h"
#include <SagaEngine/Core/Log/Log.h>
#include <SagaEngine/Startup/RuntimeStartupGate.hpp>

#include <string>
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <filesystem>
#include <optional>

namespace
{

constexpr const char* kLogTag = "Server";

struct ServerLaunchConfig
{
    Saga::TestServerConfig serverConfig;
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

bool ValidateStartupPackage(const ServerLaunchConfig& launchConfig)
{
    if (!launchConfig.packageManifestPath.has_value())
    {
        LOG_WARN(kLogTag,
                 "No --package-manifest supplied; server startup package validation is temporarily skipped for dev compatibility.");
        return true;
    }

    SagaEngine::Startup::RuntimeStartupGateOptions options;
    options.packageManifestPath = *launchConfig.packageManifestPath;
    options.expectedDomain = SagaEngine::Startup::RuntimeStartupDomain::Server;

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

static ServerLaunchConfig ParseArgs(int argc, char* argv[])
{
    ServerLaunchConfig launchConfig;

    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];

        if ((arg == "--port" || arg == "-p") && i + 1 < argc)
        {
            launchConfig.serverConfig.bindPort =
                static_cast<uint16_t>(std::atoi(argv[++i]));
        }
        else if ((arg == "--entities" || arg == "-e") && i + 1 < argc)
        {
            launchConfig.serverConfig.testEntityCount =
                static_cast<uint32_t>(std::atoi(argv[++i]));
        }
        else if ((arg == "--snap-interval" || arg == "-s") && i + 1 < argc)
        {
            launchConfig.serverConfig.snapshotIntervalSec = std::atof(argv[++i]);
        }
        else if (arg == "--package-manifest" && i + 1 < argc)
        {
            launchConfig.packageManifestPath = std::filesystem::path(argv[++i]);
        }
        else if (arg == "--help" || arg == "--?")
        {
            std::printf("Usage: SagaServer [options]\n"
                        "  --port, -p <port>         UDP port (default: 7777)\n"
                        "  --entities, -e <count>    Test entity count (default: 5)\n"
                        "  --snap-interval, -s <sec> Snapshot interval in seconds (default: 0.5)\n"
                        "  --package-manifest <path> Validate startup package manifest before initialization\n"
                        "  --help, --?               Show this help\n");
            std::exit(0);
        }
    }

    return launchConfig;
}

int main(int argc, char* argv[])
{
    SagaEngine::Core::Log::Init();

    auto launchConfig = ParseArgs(argc, argv);
    if (!ValidateStartupPackage(launchConfig))
    {
        SagaEngine::Core::Log::Shutdown();
        return 1;
    }

    auto config = launchConfig.serverConfig;

    LOG_INFO("Server", "Starting Test Snapshot Server...");
    LOG_INFO("Server", "  Port: %u", config.bindPort);
    LOG_INFO("Server", "  Entities: %u", config.testEntityCount);
    LOG_INFO("Server", "  Snapshot interval: %.2fs", config.snapshotIntervalSec);

    Saga::TestSnapshotServer server;

    if (!server.Init(config))
    {
        LOG_ERROR("Server", "Failed to initialize server.");
        return 1;
    }

    LOG_INFO("Server", "Server started. Waiting for client connections...");
    LOG_INFO("Server", "Press Ctrl+C to stop.");

    server.Start();

    // Block until interrupted.
    while (true)
    {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    server.Stop();
    SagaEngine::Core::Log::Shutdown();
    return 0;
}
