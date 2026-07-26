// SPDX-License-Identifier: MPL-2.0
/// @file RuntimeApplication.cpp
/// @brief Dispatches standalone runtime modes through Runtime-owned boundaries.

#include "RuntimeApplication.h"
#include "RuntimeAssetStartupBootstrap.hpp"
#include "RuntimeCommandLine.h"
#include "RuntimeHost.h"

#include <SagaEngine/Core/Log/Log.h>
#include <SagaEngine/Resources/AssetRegistry.h>

#include <SagaRuntime/RuntimeStartupDiagnostics.hpp>
#include <SagaRuntime/RuntimeStartupSession.hpp>

#include <cstdio>
#include <exception>
#include <filesystem>
#include <string>

namespace
{

constexpr const char* kLogTag = "Runtime";

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

        if (arg == "--headless" || arg == "-h")
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
        else if (arg == "--play-envelope" && i + 1 < argc)
        {
            commandLine.playEnvelope = std::filesystem::path(argv[++i]);
        }
        else if (arg == "--smoke-interactions")
        {
            commandLine.smokeInteractions = true;
        }
        else if (arg == "--help" || arg == "--?")
        {
            std::printf("Usage: SagaRuntime [options]\n"
                        "  --headless, -h        Run without window / renderer\n"
                        "  --package-manifest <path>\n"
                        "                         Validate startup package manifest before initialization\n"
                        "  --package-base-directory <path>\n"
                        "                         Resolve package-relative manifest and asset paths from this directory\n"
                        "  --play-envelope <path>\n"
                        "                         Run a verified local launch envelope\n"
                        "  --smoke-interactions  Inject the configured logical action twice for headless proof\n"
                        "  --help, --?           Show this help\n");
            commandLine.showHelp = true;
        }
    }

    return commandLine;
}

[[nodiscard]] bool IsValueOption(const std::string& argument) noexcept
{
    return argument == "--package-manifest" ||
           argument == "--package-base-directory" ||
           argument == "--play-envelope";
}

[[nodiscard]] bool IsFlagOption(const std::string& argument) noexcept
{
    return argument == "--headless" || argument == "-h" ||
           argument == "--smoke-interactions" ||
           argument == "--help" || argument == "--?";
}

[[nodiscard]] bool ValidateArguments(int argc,
                                     char* argv[],
                                     std::string& error)
{
    for (int i = 1; i < argc; ++i)
    {
        const std::string argument = argv[i] ? argv[i] : "";
        if (IsFlagOption(argument))
        {
            continue;
        }
        if (!IsValueOption(argument))
        {
            error = "Unknown SagaRuntime argument: " + argument;
            return false;
        }
        if (i + 1 >= argc)
        {
            error = argument + " requires a value";
            return false;
        }
        const std::string value = argv[++i] ? argv[i] : "";
        if (value.empty())
        {
            error = argument + " requires a non-empty value";
            return false;
        }
    }
    return true;
}

} // namespace

namespace SagaRuntimeApp
{

int RunRuntimeApplication(int argc, char* argv[])
{
    SagaEngine::Core::Log::Init();

    std::string argumentError;
    if (!ValidateArguments(argc, argv, argumentError))
    {
        LOG_ERROR(kLogTag, "%s", argumentError.c_str());
        SagaEngine::Core::Log::Shutdown();
        return 2;
    }

    SagaRuntimeApp::RuntimeCommandLine commandLine;
    try
    {
        commandLine = ParseArgs(argc, argv);
    }
    catch (const std::exception& error)
    {
        LOG_ERROR(kLogTag, "Invalid SagaRuntime arguments: %s", error.what());
        SagaEngine::Core::Log::Shutdown();
        return 2;
    }
    if (commandLine.showHelp)
    {
        SagaEngine::Core::Log::Shutdown();
        return 0;
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

    const SagaRuntimeApp::RuntimeHostResult hostResult =
        SagaRuntimeApp::RuntimeHost(commandLine.playEnvelope,
            commandLine.launchOptions.headless,
            commandLine.smokeInteractions).Run();
    if (!hostResult.ok)
    {
        LOG_ERROR(kLogTag,
                  "%s: %s",
                  hostResult.diagnosticId.c_str(),
                  hostResult.message.c_str());
    }
    SagaEngine::Core::Log::Shutdown();
    return hostResult.exitCode;
}

} // namespace SagaRuntimeApp
