/// @file RuntimeApplication.cpp
/// @brief Dispatches standalone runtime modes through Runtime-owned boundaries.

#include "RuntimeApplication.h"
#include "RuntimeAssetStartupBootstrap.hpp"
#include "RuntimeCommandLine.h"
#include "RuntimeHost.h"
#include "StarterArenaPlayable.h"
#include "StarterArenaSmoke.h"

#include <SagaEngine/Core/Log/Log.h>
#include <SagaEngine/Platform/PlatformFactory.h>
#include <SagaEngine/Resources/AssetRegistry.h>

#include <SagaRuntime/RuntimeStartupDiagnostics.hpp>
#include <SagaRuntime/RuntimeStartupSession.hpp>

#include <cstdint>
#include <cstdio>
#include <algorithm>
#include <array>
#include <filesystem>
#include <limits>
#include <stdexcept>
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
        else if (arg == "--project" && i + 1 < argc)
        {
            commandLine.projectPath = std::filesystem::path(argv[++i]);
        }
        else if (arg == "--starter-arena-smoke")
        {
            commandLine.starterArenaSmoke = true;
        }
        else if (arg == "--starter-arena-playable")
        {
            commandLine.starterArenaPlayable = true;
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
        else if (arg == "--run-starter-arena-gameplay")
        {
            commandLine.runStarterArenaGameplay = true;
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
        else if (arg == "--playable-frames" && i + 1 < argc)
        {
            commandLine.playableFramesProvided = true;
            commandLine.playableFrames =
                static_cast<std::uint32_t>(std::stoul(argv[++i]));
        }
        else if (arg == "--playable-report-out" && i + 1 < argc)
        {
            commandLine.playableReportOut = std::filesystem::path(argv[++i]);
        }
        else if (arg == "--playable-input-source" && i + 1 < argc)
        {
            commandLine.playableInputSource = argv[++i];
        }
        else if (arg == "--playable-input-script" && i + 1 < argc)
        {
            commandLine.playableInputScript = std::filesystem::path(argv[++i]);
        }
        else if (arg == "--fixed-dt" && i + 1 < argc)
        {
            commandLine.fixedDtSeconds = std::stod(argv[++i]);
        }
        else if (arg == "--help" || arg == "--?")
        {
            std::printf("Usage: SagaRuntime [options]\n"
                        "  --headless, -h        Run without window / renderer\n"
                        "  --package-manifest <path>\n"
                        "                         Validate startup package manifest before initialization\n"
                        "  --package-base-directory <path>\n"
                        "                         Resolve package-relative manifest and asset paths from this directory\n"
                        "  --project <path>      .sagaproj path for bounded sample smoke modes\n"
                        "  --starter-arena-smoke Run bounded local StarterArena smoke and exit\n"
                        "  --starter-arena-playable\n"
                        "                         Run the visible StarterArena frame (not headless)\n"
                        "  --script-manifest <path>\n"
                        "                         Optional StarterArena script_bindings.json smoke input\n"
                        "  --script-artifacts <path>\n"
                        "                         Optional StarterArena script_artifacts.json smoke input\n"
                        "  --invoke-starter-arena-script\n"
                        "                         Invoke controlled StarterArena AddPickupScore smoke\n"
                        "  --run-starter-arena-script-lifecycle\n"
                        "                         Invoke focused StarterArena GameRules lifecycle smoke\n"
                        "  --run-starter-arena-gameplay\n"
                        "                         Run opt-in StarterArena C# gameplay state proof\n"
                        "  --smoke-report-out <path>\n"
                        "                         Write StarterArena smoke report JSON\n"
                        "  --smoke-frames <n>    StarterArena smoke frame count (default: 30)\n"
                        "  --playable-frames <n> Bound visible StarterArena run to n presented frames\n"
                        "  --playable-report-out <path>\n"
                        "                         Write visible StarterArena report JSON\n"
                        "  --playable-input-source <scene|synthetic|keyboard>\n"
                        "                         Select visible StarterArena input (default: scene)\n"
                        "  --playable-input-script <path>\n"
                        "                         Synthetic input JSON (required for synthetic source)\n"
                        "  --fixed-dt <seconds>  StarterArena fixed timestep (default: 0.0166667)\n"
                        "  --help, --?           Show this help\n");
            commandLine.showHelp = true;
        }
    }

    return commandLine;
}

[[nodiscard]] bool IsValueOption(const std::string& argument) noexcept
{
    static constexpr std::array<const char*, 12> kOptions = {
        "--package-manifest",
        "--package-base-directory", "--project", "--script-manifest",
        "--script-artifacts", "--smoke-report-out", "--smoke-frames",
        "--playable-frames", "--playable-report-out",
        "--playable-input-source", "--playable-input-script", "--fixed-dt",
    };
    return std::find(kOptions.begin(), kOptions.end(), argument) != kOptions.end();
}

[[nodiscard]] bool IsFlagOption(const std::string& argument) noexcept
{
    static constexpr std::array<const char*, 9> kOptions = {
        "--headless", "-h", "--starter-arena-smoke",
        "--starter-arena-playable", "--invoke-starter-arena-script",
        "--run-starter-arena-script-lifecycle",
        "--run-starter-arena-gameplay", "--help", "--?",
    };
    return std::find(kOptions.begin(), kOptions.end(), argument) != kOptions.end();
}

[[nodiscard]] bool ValidateNumericValue(const std::string& option,
                                        const std::string& text,
                                        std::string& error)
{
    try
    {
        std::size_t consumed = 0;
        if (option == "--fixed-dt")
        {
            const double value = std::stod(text, &consumed);
            if (consumed != text.size() || value <= 0.0)
            {
                throw std::invalid_argument("range");
            }
            return true;
        }
        const unsigned long long value = std::stoull(text, &consumed);
        if (consumed != text.size() || value == 0)
        {
            throw std::invalid_argument("range");
        }
        if (value > std::numeric_limits<std::uint32_t>::max())
        {
            throw std::out_of_range("numeric option");
        }
        return true;
    }
    catch (const std::exception&)
    {
        error = option + " requires a positive in-range numeric value";
        return false;
    }
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
        if (argument == "--smoke-frames" ||
            argument == "--playable-frames" || argument == "--fixed-dt")
        {
            if (!ValidateNumericValue(argument, value, error))
            {
                return false;
            }
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
    if (commandLine.runStarterArenaGameplay &&
        !commandLine.runStarterArenaScriptLifecycle)
    {
        LOG_ERROR(kLogTag,
                  "--run-starter-arena-gameplay requires "
                  "--run-starter-arena-script-lifecycle.");
        SagaEngine::Core::Log::Shutdown();
        return 2;
    }
    if (!commandLine.starterArenaPlayable &&
        (commandLine.playableInputSource != "scene" ||
         !commandLine.playableInputScript.empty()))
    {
        LOG_ERROR(kLogTag,
                  "--playable-input-source and --playable-input-script require "
                  "--starter-arena-playable.");
        SagaEngine::Core::Log::Shutdown();
        return 2;
    }
    if (commandLine.starterArenaSmoke && commandLine.starterArenaPlayable)
    {
        LOG_ERROR(
            kLogTag,
            "--starter-arena-smoke and --starter-arena-playable are mutually exclusive.");
        SagaEngine::Core::Log::Shutdown();
        return 2;
    }
    if (commandLine.starterArenaSmoke)
    {
        const int result = SagaRuntimeApp::RunStarterArenaSmoke(commandLine);
        SagaEngine::Core::Log::Shutdown();
        return result;
    }

    if (commandLine.starterArenaPlayable)
    {
        auto platformFactory = Saga::CreateSDLPlatformFactory();
        Saga::PlatformFactory::Set(platformFactory.get());
        const int result = SagaRuntimeApp::RunStarterArenaPlayable(commandLine);
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

    const SagaRuntimeApp::RuntimeHostResult hostResult =
        SagaRuntimeApp::RuntimeHost{}.Run();
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
