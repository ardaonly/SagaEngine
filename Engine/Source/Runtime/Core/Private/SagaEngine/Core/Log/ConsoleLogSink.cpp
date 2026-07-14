/// @file ConsoleLogSink.cpp
/// @brief Implements stderr output for diagnostics log events.

#include <SagaEngine/Core/Log/ConsoleLogSink.hpp>

#include "SagaEngine/Core/Log/LogFormatter.hpp"

#include <iostream>

namespace SagaEngine::Core::Log
{

void ConsoleLogSink::Write(const LogEvent& event)
{
    std::cerr << Private::FormatLogEvent(event) << '\n';
}

} // namespace SagaEngine::Core::Log
