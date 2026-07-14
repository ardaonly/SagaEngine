/// @file ConsoleLogSink.hpp
/// @brief Declares stderr output for diagnostics log events.

#pragma once

#include <SagaEngine/Core/Log/LogSink.hpp>

namespace SagaEngine::Core::Log
{

/// Writes formatted diagnostic events to the process error stream.
class ConsoleLogSink final : public LogSink
{
public:
    void Write(const LogEvent& event) override;
};

} // namespace SagaEngine::Core::Log
