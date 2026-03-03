// src/SagaEngine/Core/Log/Log.cpp
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

static std::mutex g_mutex;
static std::ofstream g_file;
static Level g_level = Level::Info;
static bool g_initialized = false;

static const char* LevelToString(Level l) {
    switch (l) {
    case Level::Trace: return "TRACE";
    case Level::Debug: return "DEBUG";
    case Level::Info:  return "INFO";
    case Level::Warn:  return "WARN";
    case Level::Error: return "ERROR";
    case Level::Critical: return "CRITICAL";
    default: return "UNKNOWN";
    }
}

void Init(const std::string& filename) {
    std::lock_guard<std::mutex> lk(g_mutex);
    if (g_initialized) return;
    if (!filename.empty()) {
        g_file.open(filename, std::ios::out | std::ios::app);
    }
    g_initialized = true;
}

void Shutdown() {
    std::lock_guard<std::mutex> lk(g_mutex);
    if (g_file.is_open()) {
        g_file.flush();
        g_file.close();
    }
    g_initialized = false;
}

void SetLevel(Level level) {
    std::lock_guard<std::mutex> lk(g_mutex);
    g_level = level;
}

Level GetLevel() {
    std::lock_guard<std::mutex> lk(g_mutex);
    return g_level;
}

void Logf(Level level, const char* tag, const char* fmt, ...) {
    std::lock_guard<std::mutex> lk(g_mutex);
    if (level < g_level) return;
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm;
#if defined(_WIN32)
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    va_list args;
    va_start(args, fmt);
    std::vector<char> buf(1024);
    int needed = vsnprintf(buf.data(), buf.size(), fmt, args);
    if (needed < 0) {
        buf.assign({'\0'});
    } else if ((size_t)needed >= buf.size()) {
        buf.resize((size_t)needed + 1);
        va_end(args);
        va_start(args, fmt);
        vsnprintf(buf.data(), buf.size(), fmt, args);
    }
    va_end(args);
    std::ostringstream oss;
    oss << "[" << std::put_time(&tm, "%H:%M:%S") << "]"
        << " [" << LevelToString(level) << "]"
        << " [" << (tag ? tag : "") << "] "
        << buf.data() << "\n";
    std::fwrite(oss.str().c_str(), 1, oss.str().size(), stdout);
    std::fflush(stdout);
    if (g_file.is_open()) {
        g_file << oss.str();
        g_file.flush();
    }
}

} 
} 
}
