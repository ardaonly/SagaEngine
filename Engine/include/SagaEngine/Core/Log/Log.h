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
    Critical
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
void Logf(Level level, const char* tag, const char* fmt, ...);

// ─── Macros ───────────────────────────────────────────────────────────────────

#define LOG_TRACE(tag, fmt, ...)    ::SagaEngine::Core::Log::Logf(::SagaEngine::Core::Log::Level::Trace,    tag, fmt, ##__VA_ARGS__)
#define LOG_DEBUG(tag, fmt, ...)    ::SagaEngine::Core::Log::Logf(::SagaEngine::Core::Log::Level::Debug,    tag, fmt, ##__VA_ARGS__)
#define LOG_INFO(tag, fmt, ...)     ::SagaEngine::Core::Log::Logf(::SagaEngine::Core::Log::Level::Info,     tag, fmt, ##__VA_ARGS__)
#define LOG_WARN(tag, fmt, ...)     ::SagaEngine::Core::Log::Logf(::SagaEngine::Core::Log::Level::Warn,     tag, fmt, ##__VA_ARGS__)
#define LOG_ERROR(tag, fmt, ...)    ::SagaEngine::Core::Log::Logf(::SagaEngine::Core::Log::Level::Error,    tag, fmt, ##__VA_ARGS__)
#define LOG_CRITICAL(tag, fmt, ...) ::SagaEngine::Core::Log::Logf(::SagaEngine::Core::Log::Level::Critical, tag, fmt, ##__VA_ARGS__)

#define SAGA_LOG_TRACE    LOG_TRACE
#define SAGA_LOG_DEBUG    LOG_DEBUG
#define SAGA_LOG_INFO     LOG_INFO
#define SAGA_LOG_WARN     LOG_WARN
#define SAGA_LOG_ERROR    LOG_ERROR
#define SAGA_LOG_CRITICAL LOG_CRITICAL

} // namespace Log
} // namespace Core
} // namespace SagaEngine