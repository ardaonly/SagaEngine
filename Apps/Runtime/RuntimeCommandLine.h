/// @file RuntimeCommandLine.h
/// @brief App-local command line state shared by SagaRuntime command handlers.

#pragma once

#include <SagaRuntime/RuntimeStartupSession.hpp>

#include <cstdint>
#include <filesystem>
#include <string>

namespace SagaRuntimeApp
{

struct RuntimeCommandLine
{
    SagaRuntime::RuntimeLaunchOptions launchOptions;
    std::filesystem::path projectPath;
    std::filesystem::path smokeReportOut;
    std::filesystem::path playableReportOut;
    std::filesystem::path playableInputScript;
    std::filesystem::path scriptManifestPath;
    std::filesystem::path scriptArtifactsPath;
    bool starterArenaSmoke = false;
    bool starterArenaPlayable = false;
    bool playableFramesProvided = false;
    bool invokeStarterArenaScript = false;
    bool runStarterArenaScriptLifecycle = false;
    bool runStarterArenaGameplay = false;
    bool showHelp = false;
    std::string playableInputSource = "scene";
    std::uint32_t smokeFrames = 30;
    std::uint32_t playableFrames = 0;
    double fixedDtSeconds = 1.0 / 60.0;
};

} // namespace SagaRuntimeApp
