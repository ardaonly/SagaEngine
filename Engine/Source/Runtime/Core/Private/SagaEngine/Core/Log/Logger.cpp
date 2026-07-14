/// @file Logger.cpp
/// @brief Implements structured diagnostics logging and recent-event buffering.

#include <SagaEngine/Core/Log/Logger.hpp>

#include <chrono>
#include <utility>

namespace SagaEngine::Core::Log
{

const char* ToString(Level level) noexcept
{
    switch (level)
    {
        case Level::Trace: return "Trace";
        case Level::Debug: return "Debug";
        case Level::Info: return "Info";
        case Level::Warn: return "Warn";
        case Level::Error: return "Error";
        case Level::Critical: return "Critical";
        case Level::Fatal: return "Fatal";
    }
    return "Unknown";
}

Logger::Logger(std::size_t maxBufferedEvents)
    : maxBufferedEvents_(maxBufferedEvents)
{
}

void Logger::SetMinimumLevel(Level level) noexcept
{
    minimumLevel_.store(level);
}

Level Logger::MinimumLevel() const noexcept
{
    return minimumLevel_.load();
}

void Logger::AddSink(std::shared_ptr<LogSink> sink)
{
    if (!sink)
    {
        return;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    sinks_.push_back(std::move(sink));
}

void Logger::ClearSinks()
{
    std::lock_guard<std::mutex> lock(mutex_);
    sinks_.clear();
}

void Logger::Log(Level level,
                 std::string_view tag,
                 std::string_view message,
                 LogContext context)
{
    if (!ShouldLog(level))
    {
        return;
    }

    LogEvent event;
    event.timestamp = std::chrono::system_clock::now();
    event.level = level;
    event.tag = std::string(tag);
    event.message = std::string(message);
    event.context = std::move(context);

    std::vector<std::shared_ptr<LogSink>> sinks;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        BufferEvent(event);
        sinks = sinks_;
    }

    for (const auto& sink : sinks)
    {
        sink->Write(event);
    }
}

std::vector<LogEvent> Logger::SnapshotRecentEvents() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return recentEvents_;
}

void Logger::ClearBufferedEvents()
{
    std::lock_guard<std::mutex> lock(mutex_);
    recentEvents_.clear();
}

bool Logger::ShouldLog(Level level) const noexcept
{
    return static_cast<unsigned>(level) >= static_cast<unsigned>(minimumLevel_.load());
}

void Logger::BufferEvent(const LogEvent& event)
{
    if (maxBufferedEvents_ == 0)
    {
        return;
    }

    recentEvents_.push_back(event);
    while (recentEvents_.size() > maxBufferedEvents_)
    {
        recentEvents_.erase(recentEvents_.begin());
    }
}

} // namespace SagaEngine::Core::Log
