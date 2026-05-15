/// @file LogSink.hpp
/// @brief Declares the sink interface used by diagnostics logging.

#pragma once

#include <SagaEngine/Core/Log/LogEvent.hpp>

namespace SagaEngine::Core::Log
{

/// Receives already-filtered log events from Logger.
class LogSink
{
public:
    virtual ~LogSink() = default;
    /// Persist or display a single diagnostic event.
    virtual void Write(const LogEvent& event) = 0;
};

} // namespace SagaEngine::Core::Log
