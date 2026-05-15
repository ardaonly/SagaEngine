/// @file DiagnosticSystem.hpp
/// @brief Owns the initial logging, health, and lifetime diagnostics services.

#pragma once

#include <SagaEngine/Diagnostics/DiagnosticConfig.hpp>
#include <SagaEngine/Diagnostics/Health/HealthMonitor.hpp>
#include <SagaEngine/Diagnostics/Lifetime/LifetimeTracker.hpp>
#include <SagaEngine/Core/Log/Logger.hpp>

namespace SagaEngine::Diagnostics
{

/// Coordinates diagnostics subsystems without depending on server or SDE startup.
class DiagnosticSystem
{
public:
    /// Create diagnostics with default local-safe configuration.
    DiagnosticSystem();
    /// Create diagnostics with explicit startup configuration.
    explicit DiagnosticSystem(DiagnosticConfig config);
    ~DiagnosticSystem();

    DiagnosticSystem(const DiagnosticSystem&) = delete;
    DiagnosticSystem& operator=(const DiagnosticSystem&) = delete;

    DiagnosticSystem(DiagnosticSystem&&) = delete;
    DiagnosticSystem& operator=(DiagnosticSystem&&) = delete;

    /// Initialize configured sinks and make diagnostics available to runtime code.
    bool Start();
    /// Stop diagnostics output while preserving in-memory subsystem state.
    void Shutdown();

    [[nodiscard]] bool IsRunning() const noexcept;
    [[nodiscard]] const DiagnosticConfig& Config() const noexcept;

    [[nodiscard]] Core::Log::Logger& Log() noexcept;
    [[nodiscard]] const Core::Log::Logger& Log() const noexcept;

    [[nodiscard]] HealthMonitor& Health() noexcept;
    [[nodiscard]] const HealthMonitor& Health() const noexcept;

    [[nodiscard]] LifetimeTracker& Lifetime() noexcept;
    [[nodiscard]] const LifetimeTracker& Lifetime() const noexcept;

private:
    DiagnosticConfig config_;
    Core::Log::Logger logger_;
    HealthMonitor health_;
    LifetimeTracker lifetime_;
    bool running_ = false;
};

} // namespace SagaEngine::Diagnostics
