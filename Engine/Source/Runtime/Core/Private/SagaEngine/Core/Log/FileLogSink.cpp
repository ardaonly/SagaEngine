/// @file FileLogSink.cpp
/// @brief Implements append-only file output for diagnostics log events.

#include <SagaEngine/Core/Log/FileLogSink.hpp>

#include "SagaEngine/Core/Log/LogFormatter.hpp"

namespace SagaEngine::Core::Log
{

FileLogSink::FileLogSink(const std::string& path)
    : stream_(path, std::ios::app)
{
}

FileLogSink::~FileLogSink() = default;

void FileLogSink::Write(const LogEvent& event)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (!stream_.is_open())
    {
        return;
    }

    stream_ << Private::FormatLogEvent(event) << '\n';
    stream_.flush();
}

bool FileLogSink::IsOpen() const noexcept
{
    return stream_.is_open();
}

} // namespace SagaEngine::Core::Log
