/// @file DiagnosticConfig.hpp
/// @brief Runtime configuration for the Saga diagnostics foundation.

#pragma once

#include <SagaEngine/Core/Log/Log.h>

#include <cstddef>
#include <string>

namespace SagaEngine::Diagnostics
{

/// Startup options for the diagnostics system and its first-party sinks.
struct DiagnosticConfig
{
    bool enableLogging = true;                                      ///< Enable configured log sinks during diagnostics startup.
    bool enableHealth = true;                                       ///< Enable health ownership for future integrations.
    bool enableLifetime = true;                                     ///< Enable lifetime ownership for future integrations.
    bool enableMemoryTracking = true;                               ///< Enable explicit memory snapshot ownership.
    bool enableResourceTracking = true;                             ///< Enable explicit resource snapshot ownership.
    bool enableConsoleLog = true;                                  ///< Enable stderr logging during startup and runtime.
    bool enableFileLog = false;                                    ///< Enable file logging when logFilePath is configured.
    Core::Log::Level minimumLogLevel = Core::Log::Level::Info;     ///< Minimum severity accepted by the structured logger.
    std::string logFilePath;                                       ///< Append-only log destination used by FileLogSink.
    std::string reportDirectory = "diagnostics/reports";           ///< Deterministic folder for future diagnostic reports.
    std::size_t maxBufferedLogEvents = 1024;                       ///< Number of recent events retained for reports.
    std::size_t maxServerLifecycleEvents = 512;                    ///< Number of server lifecycle events retained for reports.
    std::size_t maxServerLifecycleRecords = 512;                   ///< Number of server lifecycle records retained for reports.
    std::size_t maxFaultReports = 128;                             ///< Number of fault reports retained for reports.
};

} // namespace SagaEngine::Diagnostics
