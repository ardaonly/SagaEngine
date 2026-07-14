/// @file DiagnosticSystem.cpp
/// @brief Implements startup and shutdown for the diagnostics foundation.

#include <SagaEngine/Diagnostics/DiagnosticSystem.hpp>
#include <SagaEngine/Core/Log/ConsoleLogSink.hpp>
#include <SagaEngine/Core/Log/FileLogSink.hpp>
#include <SagaEngine/Diagnostics/Crash/CrashReportBuilder.hpp>

#include <memory>
#include <sstream>
#include <string>
#include <thread>
#include <utility>

namespace SagaEngine::Diagnostics
{
namespace
{

[[nodiscard]] const char* ToReportMetricType(HealthMetricType type) noexcept
{
    switch (type)
    {
        case HealthMetricType::Gauge: return "gauge";
        case HealthMetricType::Counter: return "counter";
        case HealthMetricType::Timing: return "timing";
    }
    return "unknown";
}

[[nodiscard]] DiagnosticReportHealthMetric ToReportMetric(
    const HealthMetric& metric)
{
    DiagnosticReportHealthMetric result;
    result.name = metric.name;
    result.type = ToReportMetricType(metric.type);
    result.value = metric.value;
    return result;
}

[[nodiscard]] DiagnosticReportLifetimeLeak ToReportLeak(
    const LifetimeRecord& record)
{
    DiagnosticReportLifetimeLeak result;
    result.objectId = record.objectId;
    result.externalId = record.externalId;
    result.ownerObjectId = record.ownerObjectId;
    result.typeName = record.typeName;
    result.ownerSystem = record.ownerSystem;
    result.createdTick = record.createdTick;
    return result;
}

[[nodiscard]] DiagnosticReportLogEvent ToReportLogEvent(
    const Core::Log::LogEvent& event)
{
    DiagnosticReportLogEvent result;
    result.level = event.level;
    result.tag = event.tag;
    result.message = event.message;
    result.context = event.context;
    return result;
}

void RefreshSummary(DiagnosticReport& report)
{
    report.summary.healthMetricCount = report.healthMetrics.size();
    report.summary.lifetimeLeakCount = report.lifetimeLeaks.size();
    report.summary.memorySystemCount = report.memoryRecords.size();
    report.summary.activeResourceCount = report.activeResources.size();
    report.summary.recentLogEventCount = report.recentLogEvents.size();
    report.summary.faultReportCount = report.faults.summary.faultCount;
}

[[nodiscard]] DiagnosticReport MakeBaseReport(
    std::string reportKind,
    std::uint64_t generationSequence)
{
    DiagnosticReport report;
    report.reportKind = std::move(reportKind);
    report.generationSequence = generationSequence;
    return report;
}

[[nodiscard]] std::string CurrentThreadIdString()
{
    std::ostringstream stream;
    stream << std::this_thread::get_id();
    return stream.str();
}

} // namespace

DiagnosticSystem::DiagnosticSystem()
    : logger_(config_.maxBufferedLogEvents)
    , serverLifecycle_(config_.maxServerLifecycleEvents,
                       config_.maxServerLifecycleRecords)
    , faults_(config_.maxFaultReports)
{
    logger_.SetMinimumLevel(config_.minimumLogLevel);
}

DiagnosticSystem::DiagnosticSystem(DiagnosticConfig config)
    : config_(std::move(config))
    , logger_(config_.maxBufferedLogEvents)
    , serverLifecycle_(config_.maxServerLifecycleEvents,
                       config_.maxServerLifecycleRecords)
    , faults_(config_.maxFaultReports)
{
    logger_.SetMinimumLevel(config_.minimumLogLevel);
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
    logger_.SetMinimumLevel(config_.minimumLogLevel);
    if (config_.enableLogging && config_.enableConsoleLog)
    {
        logger_.AddSink(std::make_shared<Core::Log::ConsoleLogSink>());
    }
    if (config_.enableLogging && config_.enableFileLog && !config_.logFilePath.empty())
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

MemoryTracker& DiagnosticSystem::Memory() noexcept
{
    return memory_;
}

const MemoryTracker& DiagnosticSystem::Memory() const noexcept
{
    return memory_;
}

ResourceTracker& DiagnosticSystem::Resource() noexcept
{
    return resource_;
}

const ResourceTracker& DiagnosticSystem::Resource() const noexcept
{
    return resource_;
}

ServerLifecycleTracker& DiagnosticSystem::ServerLifecycle() noexcept
{
    return serverLifecycle_;
}

const ServerLifecycleTracker& DiagnosticSystem::ServerLifecycle() const noexcept
{
    return serverLifecycle_;
}

FaultTracker& DiagnosticSystem::Faults() noexcept
{
    return faults_;
}

const FaultTracker& DiagnosticSystem::Faults() const noexcept
{
    return faults_;
}

DiagnosticReport DiagnosticSystem::BuildHealthReport()
{
    auto report = MakeBaseReport(
        DiagnosticReportKinds::HealthSnapshot,
        NextReportSequence());

    const auto snapshot = health_.Snapshot();
    report.healthMetrics.reserve(snapshot.Metrics().size());
    for (const auto& metric : snapshot.Metrics())
    {
        report.healthMetrics.push_back(ToReportMetric(metric));
    }

    RefreshSummary(report);
    return report;
}

DiagnosticReport DiagnosticSystem::BuildLifetimeLeakReport()
{
    auto report = MakeBaseReport(
        DiagnosticReportKinds::LifetimeLeaks,
        NextReportSequence());

    const auto leaks = lifetime_.SnapshotActive();
    report.lifetimeLeaks.reserve(leaks.size());
    for (const auto& leak : leaks)
    {
        if (!leak.destroyed)
        {
            report.lifetimeLeaks.push_back(ToReportLeak(leak));
        }
    }

    RefreshSummary(report);
    return report;
}

DiagnosticReport DiagnosticSystem::BuildOperationalSnapshot()
{
    auto report = MakeBaseReport(
        DiagnosticReportKinds::OperationalSnapshot,
        NextReportSequence());

    const auto healthSnapshot = health_.Snapshot();
    report.healthMetrics.reserve(healthSnapshot.Metrics().size());
    for (const auto& metric : healthSnapshot.Metrics())
    {
        report.healthMetrics.push_back(ToReportMetric(metric));
    }

    const auto leaks = lifetime_.SnapshotActive();
    report.lifetimeLeaks.reserve(leaks.size());
    for (const auto& leak : leaks)
    {
        if (!leak.destroyed)
        {
            report.lifetimeLeaks.push_back(ToReportLeak(leak));
        }
    }

    const auto recentEvents = logger_.SnapshotRecentEvents();
    report.recentLogEvents.reserve(recentEvents.size());
    for (const auto& event : recentEvents)
    {
        report.recentLogEvents.push_back(ToReportLogEvent(event));
    }

    if (config_.enableMemoryTracking)
    {
        report.memoryRecords = memory_.Snapshot().Records();
    }
    if (config_.enableResourceTracking)
    {
        const auto resourceSnapshot = resource_.SnapshotActive();
        report.activeResources = resourceSnapshot.Records();
        report.resourceSummary = resourceSnapshot.BuildActiveSummary();
    }

    report.serverLifecycle = serverLifecycle_.Snapshot();
    report.faults = faults_.Snapshot();

    RefreshSummary(report);
    return report;
}

DiagnosticReportWriteResult DiagnosticSystem::WriteOperationalReport(
    const std::filesystem::path& outputPath)
{
    return DiagnosticReportWriter::WriteJsonToFile(
        BuildOperationalSnapshot(),
        outputPath);
}

CrashReport DiagnosticSystem::BuildCrashReport(
    std::string reason,
    std::map<std::string, std::string> metadata)
{
    return BuildReliabilityReport(
        CrashReportKinds::ManualCrashReport,
        std::move(reason),
        std::move(metadata));
}

CrashReport DiagnosticSystem::BuildReliabilityReport(
    std::string reportKind,
    std::string reason,
    std::map<std::string, std::string> metadata)
{
    CrashContext context;
    context.reportKind = reportKind.empty()
        ? CrashReportKinds::ReliabilityFailureReport
        : std::move(reportKind);
    context.reason = std::move(reason);
    context.threadId = CurrentThreadIdString();
    context.metadata = std::move(metadata);

    const auto healthSnapshot = health_.Snapshot();
    return CrashReportBuilder::Build(
        std::move(context),
        NextReportSequence(),
        healthSnapshot,
        health_.EvaluateRules(healthSnapshot),
        lifetime_.SnapshotActive(),
        config_.enableMemoryTracking ? memory_.Snapshot() : MemorySnapshot(),
        config_.enableResourceTracking
            ? resource_.SnapshotActive()
            : ResourceSnapshot(),
        logger_.SnapshotRecentEvents());
}

DiagnosticReportWriteResult DiagnosticSystem::WriteCrashReport(
    const std::filesystem::path& outputPath,
    std::string reason,
    std::map<std::string, std::string> metadata)
{
    return DiagnosticReportWriter::WriteJsonToFile(
        BuildCrashReport(std::move(reason), std::move(metadata)),
        outputPath);
}

DiagnosticReportWriteResult DiagnosticSystem::WriteReliabilityReport(
    const std::filesystem::path& outputPath,
    std::string reportKind,
    std::string reason,
    std::map<std::string, std::string> metadata)
{
    return DiagnosticReportWriter::WriteJsonToFile(
        BuildReliabilityReport(
            std::move(reportKind),
            std::move(reason),
            std::move(metadata)),
        outputPath);
}

std::uint64_t DiagnosticSystem::NextReportSequence() noexcept
{
    return ++reportSequence_;
}

} // namespace SagaEngine::Diagnostics
