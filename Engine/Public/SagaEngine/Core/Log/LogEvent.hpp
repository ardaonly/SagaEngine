/// @file LogEvent.hpp
/// @brief Defines structured log events and lightweight context fields.

#pragma once

#include <SagaEngine/Core/Log/Log.h>

#include <chrono>
#include <string>
#include <utility>
#include <vector>

namespace SagaEngine::Core::Log
{

/// String key-value context attached to a diagnostic log event.
using LogContext = std::vector<std::pair<std::string, std::string>>;

/// Immutable event payload delivered to sinks and retained for crash context.
struct LogEvent
{
    std::chrono::system_clock::time_point timestamp;  ///< Wall-clock event capture time.
    Level level = Level::Info;                        ///< Severity used for filtering and display.
    std::string tag;                                  ///< System or subsystem name, such as Net.Session.
    std::string message;                              ///< Human-readable event summary.
    LogContext context;                               ///< Structured fields used by reports and tools.
};

} // namespace SagaEngine::Core::Log
