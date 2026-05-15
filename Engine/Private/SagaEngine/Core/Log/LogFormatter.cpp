/// @file LogFormatter.cpp
/// @brief Implements private text formatting for diagnostics log sinks.

#include "SagaEngine/Core/Log/LogFormatter.hpp"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace SagaEngine::Core::Log::Private
{

std::string FormatLogEvent(const LogEvent& event)
{
    const auto time = std::chrono::system_clock::to_time_t(event.timestamp);
    std::tm tm{};
#if defined(_WIN32)
    localtime_s(&tm, &time);
#else
    localtime_r(&time, &tm);
#endif

    std::ostringstream out;
    out << std::put_time(&tm, "%Y-%m-%d %H:%M:%S")
        << " [" << ToString(event.level) << "]"
        << " [" << event.tag << "] "
        << event.message;

    for (const auto& [key, value] : event.context)
    {
        out << ' ' << key << '=' << value;
    }

    return out.str();
}

} // namespace SagaEngine::Core::Log::Private
