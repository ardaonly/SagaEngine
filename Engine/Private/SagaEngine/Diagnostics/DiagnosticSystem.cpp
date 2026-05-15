/// @file DiagnosticSystem.cpp
/// @brief Implements startup and shutdown for the diagnostics foundation.

#include <SagaEngine/Diagnostics/DiagnosticSystem.hpp>
#include <SagaEngine/Core/Log/ConsoleLogSink.hpp>
#include <SagaEngine/Core/Log/FileLogSink.hpp>

#include <memory>
#include <utility>

namespace SagaEngine::Diagnostics
{

DiagnosticSystem::DiagnosticSystem() = default;

DiagnosticSystem::DiagnosticSystem(DiagnosticConfig config)
    : config_(std::move(config))
    , logger_(config_.maxBufferedLogEvents)
{
}

DiagnosticSystem::~DiagnosticSystem()
{
    Shutdown();
}

bool DiagnosticSystem::Start()
{
    if (running_)
    {
        return true;
    }

    logger_.ClearSinks();
    if (config_.enableConsoleLog)
    {
        logger_.AddSink(std::make_shared<Core::Log::ConsoleLogSink>());
    }
    if (config_.enableFileLog && !config_.logFilePath.empty())
    {
        auto sink = std::make_shared<Core::Log::FileLogSink>(config_.logFilePath);
        if (!sink->IsOpen())
        {
            return false;
        }
        logger_.AddSink(std::move(sink));
    }

    running_ = true;
    return true;
}

void DiagnosticSystem::Shutdown()
{
    if (!running_)
    {
        return;
    }

    logger_.ClearSinks();
    running_ = false;
}

bool DiagnosticSystem::IsRunning() const noexcept
{
    return running_;
}

const DiagnosticConfig& DiagnosticSystem::Config() const noexcept
{
    return config_;
}

Core::Log::Logger& DiagnosticSystem::Log() noexcept
{
    return logger_;
}

const Core::Log::Logger& DiagnosticSystem::Log() const noexcept
{
    return logger_;
}

HealthMonitor& DiagnosticSystem::Health() noexcept
{
    return health_;
}

const HealthMonitor& DiagnosticSystem::Health() const noexcept
{
    return health_;
}

LifetimeTracker& DiagnosticSystem::Lifetime() noexcept
{
    return lifetime_;
}

const LifetimeTracker& DiagnosticSystem::Lifetime() const noexcept
{
    return lifetime_;
}

} // namespace SagaEngine::Diagnostics
