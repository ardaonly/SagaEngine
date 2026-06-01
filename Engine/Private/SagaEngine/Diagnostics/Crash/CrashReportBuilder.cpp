/// @file CrashReportBuilder.cpp
/// @brief Implements crash and reliability report construction.

#include <SagaEngine/Diagnostics/Crash/CrashReportBuilder.hpp>

#include <algorithm>
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

void RefreshSummary(CrashReport& report)
{
    report.summary.healthMetricCount = report.healthMetrics.size();
    report.summary.healthRuleViolationCount =
        report.healthRuleViolations.size();
    report.summary.lifetimeLeakCount = report.lifetimeLeaks.size();
    report.summary.memorySystemCount = report.memoryRecords.size();
    report.summary.activeResourceCount = report.activeResources.size();
    report.summary.recentLogEventCount = report.recentLogEvents.size();
}

} // namespace

CrashReport CrashReportBuilder::Build(
    CrashContext context,
    std::uint64_t generationSequence,
    const HealthSnapshot& healthSnapshot,
    const std::vector<HealthRuleResult>& healthRuleResults,
    const std::vector<LifetimeRecord>& lifetimeRecords,
    const MemorySnapshot& memorySnapshot,
    const ResourceSnapshot& resourceSnapshot,
    const std::vector<Core::Log::LogEvent>& recentLogEvents)
{
    CrashReport report;
    report.reportKind = std::move(context.reportKind);
    report.reason = std::move(context.reason);
    report.generationSequence = generationSequence;
    report.threadId = std::move(context.threadId);
    report.metadata = std::move(context.metadata);

    report.healthMetrics.reserve(healthSnapshot.Metrics().size());
    for (const auto& metric : healthSnapshot.Metrics())
    {
        report.healthMetrics.push_back(ToReportMetric(metric));
    }

    for (const auto& result : healthRuleResults)
    {
        if (result.evaluated && !result.passed)
        {
            report.healthRuleViolations.push_back(result);
        }
    }
    std::sort(report.healthRuleViolations.begin(),
              report.healthRuleViolations.end(),
              [](const HealthRuleResult& lhs, const HealthRuleResult& rhs) {
                  if (lhs.ruleName != rhs.ruleName)
                  {
                      return lhs.ruleName < rhs.ruleName;
                  }
                  return lhs.metricName < rhs.metricName;
              });

    const auto leakDiagnostic = BuildLifetimeLeakDiagnostic(lifetimeRecords);
    report.lifetimeLeaks = leakDiagnostic.candidates;
    report.lifetimeLeakSummary = leakDiagnostic.summary;

    report.memoryRecords = memorySnapshot.Records();
    report.activeResources = resourceSnapshot.Records();
    report.resourceSummary = resourceSnapshot.BuildActiveSummary();

    report.recentLogEvents.reserve(recentLogEvents.size());
    for (const auto& event : recentLogEvents)
    {
        report.recentLogEvents.push_back(ToReportLogEvent(event));
    }

    RefreshSummary(report);
    return report;
}

} // namespace SagaEngine::Diagnostics
