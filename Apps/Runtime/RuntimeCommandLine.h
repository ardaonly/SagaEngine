/// @file RuntimeCommandLine.h
/// @brief App-local command line state shared by SagaRuntime command handlers.

#pragma once

#include <SagaRuntime/RuntimeStartupSession.hpp>

#include <cstdint>
#include <filesystem>

namespace SagaRuntimeApp
{

struct RuntimeCommandLine
{
    SagaRuntime::RuntimeLaunchOptions launchOptions;
    std::filesystem::path projectPath;
    std::filesystem::path smokeReportOut;
    std::filesystem::path scriptManifestPath;
    std::filesystem::path scriptArtifactsPath;
    bool starterArenaSmoke = false;
    bool invokeStarterArenaScript = false;
    bool runStarterArenaScriptLifecycle = false;
    std::uint32_t smokeFrames = 30;
    double fixedDtSeconds = 1.0 / 60.0;
};

} // namespace SagaRuntimeApp
