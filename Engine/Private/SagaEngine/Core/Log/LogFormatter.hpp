/// @file LogFormatter.hpp
/// @brief Declares private formatting helpers for diagnostics log sinks.

#pragma once

#include <SagaEngine/Core/Log/LogEvent.hpp>

#include <string>

namespace SagaEngine::Core::Log::Private
{

/// Format a structured diagnostic event as a compact text log line.
[[nodiscard]] std::string FormatLogEvent(const LogEvent& event);

} // namespace SagaEngine::Core::Log::Private
