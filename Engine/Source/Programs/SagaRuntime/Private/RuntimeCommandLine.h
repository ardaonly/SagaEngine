/// @file RuntimeCommandLine.h
/// @brief App-local command line state shared by SagaRuntime command handlers.

#pragma once

#include <SagaRuntime/RuntimeStartupSession.hpp>

#include <filesystem>

namespace SagaRuntimeApp
{

struct RuntimeCommandLine
{
    SagaRuntime::RuntimeLaunchOptions launchOptions;
    std::filesystem::path playEnvelope;
    bool smokeInteractions = false;
    bool showHelp = false;
};

} // namespace SagaRuntimeApp
