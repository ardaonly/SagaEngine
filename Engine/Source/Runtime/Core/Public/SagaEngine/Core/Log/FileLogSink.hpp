/// @file FileLogSink.hpp
/// @brief Declares append-only file output for diagnostics log events.

#pragma once

#include <SagaEngine/Core/Log/LogSink.hpp>

#include <fstream>
#include <mutex>
#include <string>

namespace SagaEngine::Core::Log
{

/// Writes formatted diagnostic events to a configured log file.
class FileLogSink final : public LogSink
{
public:
    /// Open the target log file in append mode.
    explicit FileLogSink(const std::string& path);
    ~FileLogSink() override;

    FileLogSink(const FileLogSink&) = delete;
    FileLogSink& operator=(const FileLogSink&) = delete;

    void Write(const LogEvent& event) override;
    /// Return whether the sink opened its file successfully.
    [[nodiscard]] bool IsOpen() const noexcept;

private:
    mutable std::mutex mutex_;
    std::ofstream stream_;
};

} // namespace SagaEngine::Core::Log
