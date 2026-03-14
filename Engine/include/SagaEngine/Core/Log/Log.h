// include/SagaEngine/Core/Log/Log.h
#pragma once
#include <string>

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

void Init(const std::string& filename = "");
void Shutdown();
void SetLevel(Level level);
Level GetLevel();
void Logf(Level level, const char* tag, const char* fmt, ...);

#define LOG_TRACE(tag, fmt, ...) ::SagaEngine::Core::Log::Logf(::SagaEngine::Core::Log::Level::Trace, tag, fmt, ##__VA_ARGS__)
#define LOG_DEBUG(tag, fmt, ...) ::SagaEngine::Core::Log::Logf(::SagaEngine::Core::Log::Level::Debug, tag, fmt, ##__VA_ARGS__)
#define LOG_INFO(tag, fmt, ...)  ::SagaEngine::Core::Log::Logf(::SagaEngine::Core::Log::Level::Info,  tag, fmt, ##__VA_ARGS__)
#define LOG_WARN(tag, fmt, ...)  ::SagaEngine::Core::Log::Logf(::SagaEngine::Core::Log::Level::Warn,  tag, fmt, ##__VA_ARGS__)
#define LOG_ERROR(tag, fmt, ...) ::SagaEngine::Core::Log::Logf(::SagaEngine::Core::Log::Level::Error, tag, fmt, ##__VA_ARGS__)
#define LOG_CRITICAL(tag, fmt, ...) ::SagaEngine::Core::Log::Logf(::SagaEngine::Core::Log::Level::Critical, tag, fmt, ##__VA_ARGS__)

} 
} 
}
