/// @file main.cpp
/// @brief Temporary SagaRuntime adapter over the current client host.
///
/// v0.0.8 ships a role-based runtime binary while the runtime core is still
/// being separated from the older client entrypoint. Keep editor/tooling
/// dependencies out of this adapter; Runtime Core should remain separate from
/// optional client UI semantics.

#include "ClientHost.h"
#include "RuntimeAssetStartupBootstrap.hpp"
#include "RuntimeCommandLine.h"
#include "StarterArenaSmoke.h"

#include <SagaEngine/Core/Log/Log.h>
#include <SagaEngine/Platform/PlatformFactory.h>
#include <SagaEngine/Resources/AssetRegistry.h>

#include <SagaRuntime/RuntimeServiceRegistry.hpp>
#include <SagaRuntime/RuntimeServiceRegistryDiagnostics.hpp>
#include <SagaRuntime/RuntimeStartupDiagnostics.hpp>
#include <SagaRuntime/RuntimeStartupSession.hpp>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <string>
#include <utility>

namespace
{

constexpr const char* kLogTag = "Runtime";

class RuntimeBootstrapService final : public SagaRuntime::IRuntimeService
{
public:
    RuntimeBootstrapService()
    {
        m_descriptor.serviceId = "runtime.app.bootstrap";
        m_descriptor.displayName = "Runtime App Bootstrap";
        m_descriptor.category = "app";
    }

    const SagaRuntime::RuntimeServiceDescriptor& Descriptor() const noexcept override
    {
        return m_descriptor;
    }

    SagaRuntime::RuntimeServiceLifecycleResult Start() override
    {
        return SagaRuntime::RuntimeServiceLifecycle::Transition(
            m_descriptor,
            SagaRuntime::RuntimeServiceState::Registered,
            SagaRuntime::RuntimeServiceState::Started);
    }

    SagaRuntime::RuntimeServiceLifecycleResult Tick() override
    {
        SagaRuntime::RuntimeServiceLifecycleResult result;
        result.descriptor = m_descriptor;
        result.state = SagaRuntime::RuntimeServiceState::Started;
        return result;
    }

    SagaRuntime::RuntimeServiceLifecycleResult Stop() override
    {
        return SagaRuntime::RuntimeServiceLifecycle::Transition(
            m_descriptor,
            SagaRuntime::RuntimeServiceState::Started,
            SagaRuntime::RuntimeServiceState::Stopped);
    }

private:
    SagaRuntime::RuntimeServiceDescriptor m_descriptor;
};

void LogStartupDiagnostic(
    const SagaRuntime::RuntimeStartupDiagnosticView& diagnostic)
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

void LogAssetBootstrapDiagnostic(
    const SagaRuntime::RuntimeAssetBootstrapDiagnosticView& diagnostic)
{
    const std::string manifestPath = diagnostic.manifestPath.string();
    const std::string resolvedPath =
        diagnostic.resolvedPath.has_value() ? diagnostic.resolvedPath->string() : "";

    LOG_ERROR(kLogTag,
              "Startup asset bootstrap failed: %s: %s (manifest='%s')",
              diagnostic.diagnosticId.c_str(),
              diagnostic.message.c_str(),
              manifestPath.c_str());
    if (!resolvedPath.empty())
    {
        LOG_ERROR(kLogTag,
                  "Startup asset bootstrap resolved path: %s",
                  resolvedPath.c_str());
    }
}

const char* ToRuntimeServiceRegistryStateName(
    SagaRuntime::RuntimeServiceRegistryReportState state) noexcept
{
    switch (state)
    {
        case SagaRuntime::RuntimeServiceRegistryReportState::Ready:
            return "ready";
        case SagaRuntime::RuntimeServiceRegistryReportState::Blocked:
            return "blocked";
        case SagaRuntime::RuntimeServiceRegistryReportState::Idle:
            return "idle";
    }

    return "unknown";
}

void LogRuntimeServiceDiagnostic(
    const SagaRuntime::RuntimeServiceDiagnosticView& diagnostic)
{
    switch (diagnostic.severity)
    {
        case SagaRuntime::RuntimeServiceDiagnosticSeverity::Error:
            LOG_ERROR(kLogTag,
                      "Runtime service diagnostic: %s: %s",
                      diagnostic.serviceId.c_str(),
                      diagnostic.message.c_str());
            return;
        case SagaRuntime::RuntimeServiceDiagnosticSeverity::Warning:
            LOG_WARN(kLogTag,
                     "Runtime service diagnostic: %s: %s",
                     diagnostic.serviceId.c_str(),
                     diagnostic.message.c_str());
            return;
        case SagaRuntime::RuntimeServiceDiagnosticSeverity::Info:
            LOG_INFO(kLogTag,
                     "Runtime service diagnostic: %s: %s",
                     diagnostic.serviceId.c_str(),
                     diagnostic.message.c_str());
            return;
    }
}

void LogRuntimeServiceRegistryReport(
    const char* operationName,
    const SagaRuntime::RuntimeServiceRegistryReport& report)
{
    const auto summary =
        SagaRuntime::RuntimeServiceRegistryDiagnostics::Summarize(report);

    if (summary.state == SagaRuntime::RuntimeServiceRegistryReportState::Blocked)
    {
        LOG_ERROR(kLogTag,
                  "Runtime service %s blocked: services=%zu diagnostics=%zu errors=%zu warnings=%zu",
                  operationName,
                  summary.serviceResultCount,
                  summary.diagnosticCount,
                  summary.errorDiagnosticCount,
                  summary.warningDiagnosticCount);
    }
    else
    {
        LOG_INFO(kLogTag,
                 "Runtime service %s %s: services=%zu diagnostics=%zu",
                 operationName,
                 ToRuntimeServiceRegistryStateName(summary.state),
                 summary.serviceResultCount,
                 summary.diagnosticCount);
    }

    for (const SagaRuntime::RuntimeServiceDiagnosticView& diagnostic :
         SagaRuntime::RuntimeServiceRegistryDiagnostics::BuildDiagnosticViews(report))
    {
        LogRuntimeServiceDiagnostic(diagnostic);
    }
}

SagaRuntime::RuntimeServiceRegistryReport ReportForResult(
    SagaRuntime::RuntimeServiceLifecycleResult result)
{
    SagaRuntime::RuntimeServiceRegistryReport report;
    for (const SagaRuntime::RuntimeServiceDiagnostic& diagnostic :
         result.diagnostics)
    {
        report.diagnostics.push_back(diagnostic);
    }
    report.results.push_back(std::move(result));
    return report;
}

bool StartRuntimeServices(SagaRuntime::RuntimeServiceRegistry& registry)
{
    SagaRuntime::RuntimeServiceLifecycleResult registration =
        registry.Register(std::make_unique<RuntimeBootstrapService>());
    if (!registration.Succeeded())
    {
        LogRuntimeServiceRegistryReport(
            "registration",
            ReportForResult(std::move(registration)));
        return false;
    }

    SagaRuntime::RuntimeServiceRegistryReport startReport = registry.StartAll();
    LogRuntimeServiceRegistryReport("startup", startReport);
    return startReport.Succeeded();
}

void StopRuntimeServices(SagaRuntime::RuntimeServiceRegistry& registry)
{
    SagaRuntime::RuntimeServiceRegistryReport stopReport = registry.StopAll();
    LogRuntimeServiceRegistryReport("shutdown", stopReport);
}

Saga::ClientConfig ToClientConfig(
    const SagaRuntime::RuntimeSessionDescriptor& descriptor)
{
    Saga::ClientConfig config;
    config.serverHost = descriptor.serverHost;
    config.serverPort = descriptor.serverPort;
    config.headless = descriptor.headless;
    config.enableRuntimeUi = descriptor.enableStartupUi;
    config.uiContentRoot = descriptor.startupContentRoot;
    return config;
}

SagaRuntime::RuntimeStartupReport PrepareRuntimeStartup(
    const SagaRuntime::RuntimeLaunchOptions& launchOptions)
{
    auto report = SagaRuntime::RuntimeStartupSession::Prepare(launchOptions);
    const auto summary = SagaRuntime::RuntimeStartupDiagnostics::Summarize(report);
    if (summary.state == SagaRuntime::RuntimeStartupReportState::PreflightSkipped)
    {
        LOG_INFO(kLogTag,
                 "No --package-manifest supplied; skipping startup package validation for dev compatibility.");
        return report;
    }

    if (summary.state == SagaRuntime::RuntimeStartupReportState::Ready)
    {
        if (report.sessionDescriptor.packageManifestPath.has_value())
        {
            LOG_INFO(kLogTag,
                     "Startup package validation accepted '%s'.",
                     report.sessionDescriptor.packageManifestPath->string().c_str());
        }
        else
        {
            LOG_INFO(kLogTag, "Startup package validation accepted.");
        }
        return report;
    }

    for (const auto& diagnostic :
         SagaRuntime::RuntimeStartupDiagnostics::BuildDiagnosticViews(report))
    {
        LogStartupDiagnostic(diagnostic);
    }

    return report;
}

bool BootstrapRuntimeAssets(
    const SagaRuntime::RuntimeSessionDescriptor& descriptor,
    SagaEngine::Resources::AssetRegistry& assetRegistry)
{
    const auto result =
        SagaRuntimeApp::RuntimeAssetStartupBootstrap::Bootstrap(
            descriptor,
            assetRegistry);

    if (result.bootstrapSkipped)
    {
        LOG_INFO(kLogTag,
                 "No --package-manifest supplied; skipping startup asset bootstrap for dev compatibility.");
        return true;
    }

    if (result.Succeeded())
    {
        LOG_INFO(kLogTag,
                 "Startup asset bootstrap accepted '%s': registeredAssets=%zu.",
                 result.summary.packageManifestPath.string().c_str(),
                 result.summary.registeredAssetCount);
        return true;
    }

    LOG_ERROR(kLogTag,
              "Startup asset bootstrap blocked: package='%s' diagnostics=%zu errors=%zu.",
              result.summary.packageManifestPath.string().c_str(),
              result.summary.diagnosticCount,
              result.summary.errorCount);
    for (const SagaRuntime::RuntimeAssetBootstrapDiagnosticView& diagnostic :
         result.diagnostics)
    {
        LogAssetBootstrapDiagnostic(diagnostic);
    }
    return false;
}

SagaRuntimeApp::RuntimeCommandLine ParseArgs(int argc, char* argv[])
{
    SagaRuntimeApp::RuntimeCommandLine commandLine;

    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];

        if ((arg == "--server" || arg == "-s") && i + 1 < argc)
        {
            commandLine.launchOptions.serverHost = argv[++i];
        }
        else if (arg == "--port" && i + 1 < argc)
        {
            commandLine.launchOptions.serverPort =
                static_cast<std::uint16_t>(std::stoi(argv[++i]));
        }
        else if (arg == "--headless" || arg == "-h")
        {
            commandLine.launchOptions.headless = true;
        }
        else if (arg == "--package-manifest" && i + 1 < argc)
        {
            commandLine.launchOptions.packageManifestPath =
                std::filesystem::path(argv[++i]);
        }
        else if (arg == "--package-base-directory" && i + 1 < argc)
        {
            commandLine.launchOptions.packageBaseDirectory =
                std::filesystem::path(argv[++i]);
        }
        else if (arg == "--project" && i + 1 < argc)
        {
            commandLine.projectPath = std::filesystem::path(argv[++i]);
        }
        else if (arg == "--starter-arena-smoke")
        {
            commandLine.starterArenaSmoke = true;
        }
        else if (arg == "--script-manifest" && i + 1 < argc)
        {
            commandLine.scriptManifestPath = std::filesystem::path(argv[++i]);
        }
        else if (arg == "--script-artifacts" && i + 1 < argc)
        {
            commandLine.scriptArtifactsPath = std::filesystem::path(argv[++i]);
        }
        else if (arg == "--invoke-starter-arena-script")
        {
            commandLine.invokeStarterArenaScript = true;
        }
        else if (arg == "--run-starter-arena-script-lifecycle")
        {
            commandLine.runStarterArenaScriptLifecycle = true;
        }
        else if (arg == "--smoke-report-out" && i + 1 < argc)
        {
            commandLine.smokeReportOut = std::filesystem::path(argv[++i]);
        }
        else if (arg == "--smoke-frames" && i + 1 < argc)
        {
            commandLine.smokeFrames =
                static_cast<std::uint32_t>(std::stoul(argv[++i]));
        }
        else if (arg == "--fixed-dt" && i + 1 < argc)
        {
            commandLine.fixedDtSeconds = std::stod(argv[++i]);
        }
        else if (arg == "--help" || arg == "--?")
        {
            std::printf("Usage: SagaRuntime [options]\n"
                        "  --server, -s <host>   Server address (default: 127.0.0.1)\n"
                        "  --port <port>         Server port (default: 7777)\n"
                        "  --headless, -h        Run without window / renderer\n"
                        "  --package-manifest <path>\n"
                        "                         Validate startup package manifest before initialization\n"
                        "  --package-base-directory <path>\n"
                        "                         Resolve package-relative manifest and asset paths from this directory\n"
                        "  --project <path>      .sagaproj path for bounded sample smoke modes\n"
                        "  --starter-arena-smoke Run bounded local StarterArena smoke and exit\n"
                        "  --script-manifest <path>\n"
                        "                         Optional StarterArena script_bindings.json smoke input\n"
                        "  --script-artifacts <path>\n"
                        "                         Optional StarterArena script_artifacts.json smoke input\n"
                        "  --invoke-starter-arena-script\n"
                        "                         Invoke controlled StarterArena AddPickupScore smoke\n"
                        "  --run-starter-arena-script-lifecycle\n"
                        "                         Invoke focused StarterArena GameRules lifecycle smoke\n"
                        "  --smoke-report-out <path>\n"
                        "                         Write StarterArena smoke report JSON\n"
                        "  --smoke-frames <n>    StarterArena smoke frame count (default: 30)\n"
                        "  --fixed-dt <seconds>  StarterArena fixed timestep (default: 0.0166667)\n"
                        "  --help, --?           Show this help\n");
            std::exit(0);
        }
    }

    return commandLine;
}

} // namespace

int main(int argc, char* argv[])
{
    SagaEngine::Core::Log::Init();

    auto commandLine = ParseArgs(argc, argv);
    if (commandLine.starterArenaSmoke)
    {
        const int result = SagaRuntimeApp::RunStarterArenaSmoke(commandLine);
        SagaEngine::Core::Log::Shutdown();
        return result;
    }

    auto startupReport = PrepareRuntimeStartup(commandLine.launchOptions);
    if (!startupReport.Succeeded())
    {
        SagaEngine::Core::Log::Shutdown();
        return 1;
    }

    SagaEngine::Resources::AssetRegistry assetRegistry;
    if (!BootstrapRuntimeAssets(
            startupReport.sessionDescriptor,
            assetRegistry))
    {
        SagaEngine::Core::Log::Shutdown();
        return 1;
    }

    SagaRuntime::RuntimeServiceRegistry runtimeServices;
    if (!StartRuntimeServices(runtimeServices))
    {
        SagaEngine::Core::Log::Shutdown();
        return 1;
    }

    auto platformFactory = Saga::CreateSDLPlatformFactory();
    Saga::PlatformFactory::Set(platformFactory.get());

    Saga::ClientHost host(ToClientConfig(startupReport.sessionDescriptor));
    host.Run();

    StopRuntimeServices(runtimeServices);

    SagaEngine::Core::Log::Shutdown();
    return 0;
}
