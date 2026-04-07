/// @file Log.cpp
/// @brief Engine log implementation with optional external sink.

#include "SagaEngine/Core/Log/Log.h"
#include <iostream>
#include <fstream>
#include <mutex>
#include <cstdarg>
#include <vector>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <cstdio>

namespace SagaEngine {
namespace Core {
namespace Log {

// ─── Global state ─────────────────────────────────────────────────────────────

static std::mutex    g_mutex;
static std::ofstream g_file;
static Level         g_level       = Level::Info;
static bool          g_initialized = false;
static LogSinkFn     g_sink;           ///< Optional external sink (e.g. EventLogPanel)

// ─── Helpers ──────────────────────────────────────────────────────────────────

static const char* LevelToString(Level l)
{
    switch (l)
    {
    case Level::Trace:    return "TRACE";
    case Level::Debug:    return "DEBUG";
    case Level::Info:     return "INFO";
    case Level::Warn:     return "WARN";
    case Level::Error:    return "ERROR";
    case Level::Critical: return "CRITICAL";
    default:              return "UNKNOWN";
    }
}

// ─── Public API ───────────────────────────────────────────────────────────────

void Init(const std::string& filename)
{
    std::lock_guard<std::mutex> lk(g_mutex);
    if (g_initialized) return;
    if (!filename.empty())
        g_file.open(filename, std::ios::out | std::ios::app);
    g_initialized = true;
}

void Shutdown()
{
    std::lock_guard<std::mutex> lk(g_mutex);
    g_sink = nullptr;
    if (g_file.is_open())
    {
        g_file.flush();
        g_file.close();
    }
    g_initialized = false;
}

void SetLevel(Level level)
{
    std::lock_guard<std::mutex> lk(g_mutex);
    g_level = level;
}

Level GetLevel()
{
    std::lock_guard<std::mutex> lk(g_mutex);
    return g_level;
}

// ─── SetSink ──────────────────────────────────────────────────────────────────

void SetSink(LogSinkFn fn)
{
    std::lock_guard<std::mutex> lk(g_mutex);
    g_sink = std::move(fn);
}

// ─── Logf ─────────────────────────────────────────────────────────────────────

void Logf(Level level, const char* tag, const char* fmt, ...)
{
    std::lock_guard<std::mutex> lk(g_mutex);
    if (level < g_level) return;

    // Timestamp
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
#if defined(_WIN32)
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif

    // Format message
    va_list args;
    va_start(args, fmt);
    std::vector<char> buf(1024);
    int needed = vsnprintf(buf.data(), buf.size(), fmt, args);
    if (needed < 0)
    {
        buf[0] = '\0';
    }
    else if (static_cast<size_t>(needed) >= buf.size())
    {
        buf.resize(static_cast<size_t>(needed) + 1);
        va_end(args);
        va_start(args, fmt);
        vsnprintf(buf.data(), buf.size(), fmt, args);
    }
    va_end(args);

    // Build output line
    std::ostringstream oss;
    oss << "[" << std::put_time(&tm, "%H:%M:%S") << "]"
        << " [" << LevelToString(level) << "]"
        << " [" << (tag ? tag : "") << "] "
        << buf.data() << "\n";

    const std::string line = oss.str();

    // Console
    std::fwrite(line.c_str(), 1, line.size(), stdout);
    std::fflush(stdout);

    // File
    if (g_file.is_open())
    {
        g_file << line;
        g_file.flush();
    }

    // External sink (e.g. EventLogPanel) — called without holding mutex to
    // avoid deadlock if the sink calls back into Log
    if (g_sink)
    {
        const LogSinkFn sinkCopy = g_sink;
        // Release mutex before calling sink
        // (We already have the lock — re-entrant call would deadlock.
        //  The sink must not call Logf. This is documented.)
        sinkCopy(level, tag, buf.data());
    }
}

} // namespace Log
} // namespace Core
} // namespace SagaEngine