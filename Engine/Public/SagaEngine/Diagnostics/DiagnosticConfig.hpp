/// @file DiagnosticConfig.hpp
/// @brief Runtime configuration for the Saga diagnostics foundation.

#pragma once

#include <cstddef>
#include <string>

namespace SagaEngine::Diagnostics
{

/// Startup options for the diagnostics system and its first-party sinks.
struct DiagnosticConfig
{
    bool enableConsoleLog = true;                             ///< Enable stderr logging during startup and runtime.
    bool enableFileLog = false;                               ///< Enable file logging when logFilePath is configured.
    std::string logFilePath;                                  ///< Append-only log destination used by FileLogSink.
    std::string reportDirectory = "diagnostics/reports";      ///< Deterministic folder for future diagnostic reports.
    std::size_t maxBufferedLogEvents = 1024;                  ///< Number of recent events retained for reports.
};

} // namespace SagaEngine::Diagnostics
