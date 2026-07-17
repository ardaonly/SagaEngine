/// @file RuntimeCommandLine.h
/// @brief App-local command line state shared by SagaRuntime command handlers.

#pragma once

#include <SagaRuntime/RuntimeStartupSession.hpp>

namespace SagaRuntimeApp
{

struct RuntimeCommandLine
{
    SagaRuntime::RuntimeLaunchOptions launchOptions;
    bool showHelp = false;
};

} // namespace SagaRuntimeApp
