/// @file DiagnosticSystem.hpp
/// @brief Owns the initial logging, health, and lifetime diagnostics services.

#pragma once

#include <SagaEngine/Diagnostics/DiagnosticConfig.hpp>
#include <SagaEngine/Diagnostics/Fault/FaultBoundary.hpp>
#include <SagaEngine/Diagnostics/Health/HealthMonitor.hpp>
#include <SagaEngine/Diagnostics/Lifetime/LifetimeTracker.hpp>
#include <SagaEngine/Diagnostics/Memory/MemoryTracker.hpp>
#include <SagaEngine/Diagnostics/Report/DiagnosticReport.hpp>
#include <SagaEngine/Diagnostics/Report/DiagnosticReportWriter.hpp>
#include <SagaEngine/Diagnostics/Resource/ResourceTracker.hpp>
#include <SagaEngine/Diagnostics/ServerLifecycle/ServerLifecycleTracker.hpp>
#include <SagaEngine/Core/Log/Logger.hpp>

#include <cstdint>
#include <filesystem>
#include <map>
#include <string>

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

    [[nodiscard]] MemoryTracker& Memory() noexcept;
    [[nodiscard]] const MemoryTracker& Memory() const noexcept;

    [[nodiscard]] ResourceTracker& Resource() noexcept;
    [[nodiscard]] const ResourceTracker& Resource() const noexcept;

    [[nodiscard]] ServerLifecycleTracker& ServerLifecycle() noexcept;
    [[nodiscard]] const ServerLifecycleTracker& ServerLifecycle() const noexcept;

    [[nodiscard]] FaultTracker& Faults() noexcept;
    [[nodiscard]] const FaultTracker& Faults() const noexcept;

    [[nodiscard]] DiagnosticReport BuildHealthReport();
    [[nodiscard]] DiagnosticReport BuildLifetimeLeakReport();
    [[nodiscard]] DiagnosticReport BuildOperationalSnapshot();
    [[nodiscard]] DiagnosticReportWriteResult WriteOperationalReport(
        const std::filesystem::path& outputPath);
    [[nodiscard]] CrashReport BuildCrashReport(
        std::string reason,
        std::map<std::string, std::string> metadata = {});
    [[nodiscard]] CrashReport BuildReliabilityReport(
        std::string reportKind,
        std::string reason,
        std::map<std::string, std::string> metadata = {});
    [[nodiscard]] DiagnosticReportWriteResult WriteCrashReport(
        const std::filesystem::path& outputPath,
        std::string reason,
        std::map<std::string, std::string> metadata = {});
    [[nodiscard]] DiagnosticReportWriteResult WriteReliabilityReport(
        const std::filesystem::path& outputPath,
        std::string reportKind,
        std::string reason,
        std::map<std::string, std::string> metadata = {});

private:
    [[nodiscard]] std::uint64_t NextReportSequence() noexcept;

    DiagnosticConfig config_;
    Core::Log::Logger logger_;
    ServerLifecycleTracker serverLifecycle_;
    FaultTracker faults_;
    HealthMonitor health_;
    LifetimeTracker lifetime_;
    MemoryTracker memory_;
    ResourceTracker resource_;
    std::uint64_t reportSequence_ = 0;
    bool running_ = false;
};

} // namespace SagaEngine::Diagnostics
