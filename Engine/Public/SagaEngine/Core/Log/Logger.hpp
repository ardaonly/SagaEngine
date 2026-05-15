/// @file Logger.hpp
/// @brief Provides structured runtime logging with bounded recent-event storage.

#pragma once

#include <SagaEngine/Core/Log/LogEvent.hpp>
#include <SagaEngine/Core/Log/LogSink.hpp>

#include <atomic>
#include <cstddef>
#include <memory>
#include <mutex>
#include <string_view>
#include <vector>

namespace SagaEngine::Core::Log
{

/// Dispatches structured events to sinks and keeps recent events for reports.
class Logger
{
public:
    /// Create a logger with a bounded recent-event buffer.
    explicit Logger(std::size_t maxBufferedEvents = 1024);

    void SetMinimumLevel(Level level) noexcept;
    [[nodiscard]] Level MinimumLevel() const noexcept;

    /// Add a sink that will receive future events.
    void AddSink(std::shared_ptr<LogSink> sink);
    /// Remove all registered sinks.
    void ClearSinks();

    /// Publish a structured log event if it passes the current severity threshold.
    void Log(Level level,
             std::string_view tag,
             std::string_view message,
             LogContext context = {});

    [[nodiscard]] std::vector<LogEvent> SnapshotRecentEvents() const;
    void ClearBufferedEvents();

private:
    [[nodiscard]] bool ShouldLog(Level level) const noexcept;
    void BufferEvent(const LogEvent& event);

    mutable std::mutex mutex_;
    std::vector<std::shared_ptr<LogSink>> sinks_;
    std::vector<LogEvent> recentEvents_;
    std::size_t maxBufferedEvents_ = 1024;
    std::atomic<Level> minimumLevel_ = Level::Info;
};

} // namespace SagaEngine::Core::Log
