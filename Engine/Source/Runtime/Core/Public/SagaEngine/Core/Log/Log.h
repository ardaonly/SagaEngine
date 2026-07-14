/// @file Log.h
/// @brief Log System.

#pragma once
#include <string>
#include <functional>

namespace SagaEngine {
namespace Core {
namespace Log {

enum class Level {
    Trace = 0,
    Debug,
    Info,
    Warn,
    Error,
    Critical,
    Fatal
};

// ─── Sink API ─────────────────────────────────────────────────────────────────

/// External log sink — called after normal console/file output.
/// The sink is called with the mutex released to avoid re-entrant deadlock.
/// IMPORTANT: The sink function must NOT call Logf() or any LOG_* macro.
using LogSinkFn = std::function<void(Level level, const char* tag, const char* message)>;

/// Register a global log sink. Pass nullptr to remove.
/// Thread-safe. Only one sink supported at a time.
void SetSink(LogSinkFn fn);

// ─── Core functions ───────────────────────────────────────────────────────────

void Init(const std::string& filename = "");
void Shutdown();
void SetLevel(Level level);
Level GetLevel();
[[nodiscard]] const char* ToString(Level level) noexcept;
void Logf(Level level, const char* tag, const char* fmt, ...);

// ─── Macros ───────────────────────────────────────────────────────────────────

#define SAGA_LOG_TRACE(tag, fmt, ...)    ::SagaEngine::Core::Log::Logf(::SagaEngine::Core::Log::Level::Trace,    tag, fmt, ##__VA_ARGS__)
#define SAGA_LOG_DEBUG(tag, fmt, ...)    ::SagaEngine::Core::Log::Logf(::SagaEngine::Core::Log::Level::Debug,    tag, fmt, ##__VA_ARGS__)
#define SAGA_LOG_INFO(tag, fmt, ...)     ::SagaEngine::Core::Log::Logf(::SagaEngine::Core::Log::Level::Info,     tag, fmt, ##__VA_ARGS__)
#define SAGA_LOG_WARN(tag, fmt, ...)     ::SagaEngine::Core::Log::Logf(::SagaEngine::Core::Log::Level::Warn,     tag, fmt, ##__VA_ARGS__)
#define SAGA_LOG_ERROR(tag, fmt, ...)    ::SagaEngine::Core::Log::Logf(::SagaEngine::Core::Log::Level::Error,    tag, fmt, ##__VA_ARGS__)
#define SAGA_LOG_CRITICAL(tag, fmt, ...) ::SagaEngine::Core::Log::Logf(::SagaEngine::Core::Log::Level::Critical, tag, fmt, ##__VA_ARGS__)
#define SAGA_LOG_FATAL(tag, fmt, ...)    ::SagaEngine::Core::Log::Logf(::SagaEngine::Core::Log::Level::Fatal,    tag, fmt, ##__VA_ARGS__)

#ifndef LOG_TRACE
#define LOG_TRACE SAGA_LOG_TRACE
#endif
#ifndef LOG_DEBUG
#define LOG_DEBUG SAGA_LOG_DEBUG
#endif
#ifndef LOG_INFO
#define LOG_INFO SAGA_LOG_INFO
#endif
#ifndef LOG_WARN
#define LOG_WARN SAGA_LOG_WARN
#endif
#ifndef LOG_ERROR
#define LOG_ERROR SAGA_LOG_ERROR
#endif
#ifndef LOG_CRITICAL
#define LOG_CRITICAL SAGA_LOG_CRITICAL
#endif
#ifndef LOG_FATAL
#define LOG_FATAL SAGA_LOG_FATAL
#endif

} // namespace Log
} // namespace Core
} // namespace SagaEngine
