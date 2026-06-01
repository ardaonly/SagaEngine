/// @file DiagnosticReportWriter.cpp
/// @brief Implements deterministic diagnostics report JSON writing.

#include <SagaEngine/Diagnostics/Report/DiagnosticReportWriter.hpp>

#include <nlohmann/json.hpp>

#include <fstream>
#include <sstream>
#include <system_error>
#include <utility>

namespace SagaEngine::Diagnostics
{
namespace
{

using Json = nlohmann::ordered_json;

[[nodiscard]] DiagnosticReportWriteDiagnostic MakeDiagnostic(
    std::string diagnosticId,
    std::string message,
    std::optional<std::filesystem::path> outputPath = std::nullopt)
{
    DiagnosticReportWriteDiagnostic diagnostic;
    diagnostic.diagnosticId = std::move(diagnosticId);
    diagnostic.message = std::move(message);
    diagnostic.outputPath = std::move(outputPath);
    return diagnostic;
}

[[nodiscard]] Json BuildSummaryJson(const DiagnosticReportSummary& summary)
{
    return Json{
        {"healthMetricCount", summary.healthMetricCount},
        {"lifetimeLeakCount", summary.lifetimeLeakCount},
        {"memorySystemCount", summary.memorySystemCount},
        {"activeResourceCount", summary.activeResourceCount},
        {"recentLogEventCount", summary.recentLogEventCount},
        {"faultReportCount", summary.faultReportCount},
    };
}

[[nodiscard]] Json BuildHealthMetricJson(
    const DiagnosticReportHealthMetric& metric)
{
    return Json{
        {"name", metric.name},
        {"type", metric.type},
        {"value", metric.value},
    };
}

[[nodiscard]] Json BuildLifetimeLeakJson(
    const DiagnosticReportLifetimeLeak& leak)
{
    return Json{
        {"objectId", leak.objectId},
        {"externalId", leak.externalId},
        {"ownerObjectId", leak.ownerObjectId},
        {"typeName", leak.typeName},
        {"ownerSystem", leak.ownerSystem},
        {"createdTick", leak.createdTick},
    };
}

[[nodiscard]] Json BuildLogEventJson(const DiagnosticReportLogEvent& event)
{
    Json context = Json::array();
    for (const auto& [key, value] : event.context)
    {
        context.push_back(Json{
            {"key", key},
            {"value", value},
        });
    }

    return Json{
        {"level", Core::Log::ToString(event.level)},
        {"tag", event.tag},
        {"message", event.message},
        {"context", std::move(context)},
    };
}

[[nodiscard]] Json BuildHealthRuleViolationJson(
    const HealthRuleResult& result)
{
    return Json{
        {"ruleName", result.ruleName},
        {"metricName", result.metricName},
        {"type", ToString(result.type)},
        {"severity", ToString(result.severity)},
        {"threshold", result.threshold},
        {"observedValue", result.observedValue},
        {"evaluated", result.evaluated},
        {"passed", result.passed},
        {"message", result.message},
    };
}

[[nodiscard]] Json BuildLifetimeLeakSummaryJson(
    const LifetimeLeakSummary& summary)
{
    Json byType = Json::object();
    for (const auto& [typeName, count] : summary.byType)
    {
        byType[typeName] = count;
    }

    Json byOwnerSystem = Json::object();
    for (const auto& [ownerSystem, count] : summary.byOwnerSystem)
    {
        byOwnerSystem[ownerSystem] = count;
    }

    return Json{
        {"totalActive", summary.totalActive},
        {"byType", std::move(byType)},
        {"byOwnerSystem", std::move(byOwnerSystem)},
    };
}

[[nodiscard]] Json BuildLifetimeLeakCandidateJson(
    const LifetimeLeakCandidate& leak)
{
    return Json{
        {"objectId", leak.objectId},
        {"externalId", leak.externalId},
        {"ownerObjectId", leak.ownerObjectId},
        {"typeName", leak.typeName},
        {"ownerSystem", leak.ownerSystem},
        {"createdTick", leak.createdTick},
    };
}

[[nodiscard]] Json BuildMetadataJson(
    const std::map<std::string, std::string>& metadata)
{
    Json result = Json::object();
    for (const auto& [key, value] : metadata)
    {
        result[key] = value;
    }
    return result;
}

[[nodiscard]] Json BuildMemoryRecordJson(const MemoryRecord& record)
{
    return Json{
        {"systemName", record.systemName},
        {"currentBytes", record.currentBytes},
        {"peakBytes", record.peakBytes},
        {"totalAllocatedBytes", record.totalAllocatedBytes},
        {"totalFreedBytes", record.totalFreedBytes},
        {"activeAllocationCount", record.activeAllocationCount},
    };
}

[[nodiscard]] Json BuildResourceSummaryJson(
    const ResourceLeakSummary& summary)
{
    Json byType = Json::object();
    for (const auto& [typeName, count] : summary.byType)
    {
        byType[typeName] = count;
    }

    Json byOwnerSystem = Json::object();
    for (const auto& [ownerSystem, count] : summary.byOwnerSystem)
    {
        byOwnerSystem[ownerSystem] = count;
    }

    return Json{
        {"totalActive", summary.totalActive},
        {"byType", std::move(byType)},
        {"byOwnerSystem", std::move(byOwnerSystem)},
    };
}

[[nodiscard]] Json BuildResourceRecordJson(const ResourceRecord& record)
{
    return Json{
        {"resourceId", record.resourceId},
        {"resourceType", ToString(record.resourceType)},
        {"ownerSystem", record.ownerSystem},
        {"debugName", record.debugName},
        {"createdTick", record.createdTick},
        {"releasedTick", record.releasedTick},
        {"released", record.released},
    };
}

[[nodiscard]] Json BuildServerLifecycleEventJson(
    const ServerLifecycleEvent& event)
{
    Json payload = Json::array();
    for (const auto& [key, value] : event.payload)
    {
        payload.push_back(Json{
            {"key", key},
            {"value", value},
        });
    }

    return Json{
        {"sequence", event.sequence},
        {"eventName", event.eventName},
        {"category", event.category},
        {"severity", event.severity},
        {"message", event.message},
        {"zoneId", event.zoneId},
        {"tick", event.tick},
        {"payload", std::move(payload)},
    };
}

[[nodiscard]] Json BuildServerLifecycleRecordJson(
    const ServerLifecycleRecord& record)
{
    return Json{
        {"recordId", record.recordId},
        {"recordKind", record.recordKind},
        {"zoneId", record.zoneId},
        {"externalId", record.externalId},
        {"ownerRecordId", record.ownerRecordId},
        {"createdTick", record.createdTick},
        {"destroyedTick", record.destroyedTick},
        {"active", record.active},
        {"status", record.status},
        {"label", record.label},
    };
}

[[nodiscard]] Json BuildServerLifecycleSummaryJson(
    const ServerLifecycleSummary& summary)
{
    return Json{
        {"eventCount", summary.eventCount},
        {"recordCount", summary.recordCount},
        {"activeRecordCount", summary.activeRecordCount},
        {"leakCount", summary.leakCount},
        {"droppedEventCount", summary.droppedEventCount},
        {"droppedRecordCount", summary.droppedRecordCount},
    };
}

[[nodiscard]] Json BuildServerLifecycleJson(
    const ServerLifecycleSnapshot& snapshot)
{
    Json events = Json::array();
    for (const auto& event : snapshot.events)
    {
        events.push_back(BuildServerLifecycleEventJson(event));
    }

    Json records = Json::array();
    for (const auto& record : snapshot.records)
    {
        records.push_back(BuildServerLifecycleRecordJson(record));
    }

    Json leaks = Json::array();
    for (const auto& leak : snapshot.leaks)
    {
        leaks.push_back(BuildServerLifecycleRecordJson(leak));
    }

    return Json{
        {"events", std::move(events)},
        {"records", std::move(records)},
        {"leaks", std::move(leaks)},
        {"summary", BuildServerLifecycleSummaryJson(snapshot.summary)},
    };
}

[[nodiscard]] Json BuildFaultMetadataJson(
    const std::vector<std::pair<std::string, std::string>>& metadata)
{
    Json result = Json::array();
    for (const auto& [key, value] : metadata)
    {
        result.push_back(Json{
            {"key", key},
            {"value", value},
        });
    }
    return result;
}

[[nodiscard]] Json BuildFaultReportJson(const FaultReport& report)
{
    return Json{
        {"sequence", report.sequence},
        {"faultId", report.faultId},
        {"subsystem", report.subsystem},
        {"severity", ToString(report.severity)},
        {"policy", ToString(report.policy)},
        {"recommendedAction", ToString(report.recommendedAction)},
        {"message", report.message},
        {"diagnosticCode", report.diagnosticCode},
        {"metadata", BuildFaultMetadataJson(report.metadata)},
    };
}

[[nodiscard]] Json BuildFaultSummaryJson(const FaultReportSummary& summary)
{
    return Json{
        {"faultCount", summary.faultCount},
        {"droppedFaultCount", summary.droppedFaultCount},
    };
}

[[nodiscard]] Json BuildFaultSnapshotJson(const FaultSnapshot& snapshot)
{
    Json reports = Json::array();
    for (const auto& report : snapshot.reports)
    {
        reports.push_back(BuildFaultReportJson(report));
    }

    return Json{
        {"reports", std::move(reports)},
        {"summary", BuildFaultSummaryJson(snapshot.summary)},
    };
}

[[nodiscard]] Json BuildReportJson(const DiagnosticReport& report)
{
    Json healthMetrics = Json::array();
    for (const auto& metric : report.healthMetrics)
    {
        healthMetrics.push_back(BuildHealthMetricJson(metric));
    }

    Json lifetimeLeaks = Json::array();
    for (const auto& leak : report.lifetimeLeaks)
    {
        lifetimeLeaks.push_back(BuildLifetimeLeakJson(leak));
    }

    Json recentLogEvents = Json::array();
    for (const auto& event : report.recentLogEvents)
    {
        recentLogEvents.push_back(BuildLogEventJson(event));
    }

    Json memoryRecords = Json::array();
    for (const auto& record : report.memoryRecords)
    {
        memoryRecords.push_back(BuildMemoryRecordJson(record));
    }

    Json activeResources = Json::array();
    for (const auto& record : report.activeResources)
    {
        activeResources.push_back(BuildResourceRecordJson(record));
    }

    return Json{
        {"schemaVersion", report.schemaVersion},
        {"reportKind", report.reportKind},
        {"generationSequence", report.generationSequence},
        {"summary", BuildSummaryJson(report.summary)},
        {"healthMetrics", std::move(healthMetrics)},
        {"lifetimeLeaks", std::move(lifetimeLeaks)},
        {"memoryRecords", std::move(memoryRecords)},
        {"resourceSummary", BuildResourceSummaryJson(report.resourceSummary)},
        {"activeResources", std::move(activeResources)},
        {"recentLogEvents", std::move(recentLogEvents)},
        {"serverLifecycle", BuildServerLifecycleJson(report.serverLifecycle)},
        {"faults", BuildFaultSnapshotJson(report.faults)},
        {"metadata", BuildMetadataJson(report.metadata)},
    };
}

[[nodiscard]] Json BuildCrashSummaryJson(const CrashReportSummary& summary)
{
    return Json{
        {"healthMetricCount", summary.healthMetricCount},
        {"healthRuleViolationCount", summary.healthRuleViolationCount},
        {"lifetimeLeakCount", summary.lifetimeLeakCount},
        {"memorySystemCount", summary.memorySystemCount},
        {"activeResourceCount", summary.activeResourceCount},
        {"recentLogEventCount", summary.recentLogEventCount},
    };
}

[[nodiscard]] Json BuildReportJson(const CrashReport& report)
{
    Json healthMetrics = Json::array();
    for (const auto& metric : report.healthMetrics)
    {
        healthMetrics.push_back(BuildHealthMetricJson(metric));
    }

    Json healthRuleViolations = Json::array();
    for (const auto& violation : report.healthRuleViolations)
    {
        healthRuleViolations.push_back(
            BuildHealthRuleViolationJson(violation));
    }

    Json lifetimeLeaks = Json::array();
    for (const auto& leak : report.lifetimeLeaks)
    {
        lifetimeLeaks.push_back(BuildLifetimeLeakCandidateJson(leak));
    }

    Json recentLogEvents = Json::array();
    for (const auto& event : report.recentLogEvents)
    {
        recentLogEvents.push_back(BuildLogEventJson(event));
    }

    Json memoryRecords = Json::array();
    for (const auto& record : report.memoryRecords)
    {
        memoryRecords.push_back(BuildMemoryRecordJson(record));
    }

    Json activeResources = Json::array();
    for (const auto& record : report.activeResources)
    {
        activeResources.push_back(BuildResourceRecordJson(record));
    }

    Json root = {
        {"schemaVersion", report.schemaVersion},
        {"reportKind", report.reportKind},
        {"reason", report.reason},
        {"generationSequence", report.generationSequence},
        {"threadId", report.threadId.has_value() ? Json(*report.threadId) : Json(nullptr)},
        {"summary", BuildCrashSummaryJson(report.summary)},
        {"healthMetrics", std::move(healthMetrics)},
        {"healthRuleViolations", std::move(healthRuleViolations)},
        {"lifetimeLeaks", std::move(lifetimeLeaks)},
        {"lifetimeLeakSummary",
         BuildLifetimeLeakSummaryJson(report.lifetimeLeakSummary)},
        {"memoryRecords", std::move(memoryRecords)},
        {"resourceSummary", BuildResourceSummaryJson(report.resourceSummary)},
        {"activeResources", std::move(activeResources)},
        {"recentLogEvents", std::move(recentLogEvents)},
        {"metadata", BuildMetadataJson(report.metadata)},
    };
    return root;
}

} // namespace

DiagnosticReportWriteResult DiagnosticReportWriter::WriteJson(
    const DiagnosticReport& report,
    std::ostream& output)
{
    DiagnosticReportWriteResult result;
    const std::string payload = BuildReportJson(report).dump(2) + '\n';

    output << payload;
    if (!output.good())
    {
        result.diagnostics.push_back(MakeDiagnostic(
            DiagnosticReportWriterDiagnostics::OutputWriteFailed,
            "Diagnostics report JSON could not be written."));
        return result;
    }

    result.bytesWritten = payload.size();
    return result;
}

DiagnosticReportWriteResult DiagnosticReportWriter::WriteJson(
    const CrashReport& report,
    std::ostream& output)
{
    DiagnosticReportWriteResult result;
    const std::string payload = BuildReportJson(report).dump(2) + '\n';

    output << payload;
    if (!output.good())
    {
        result.diagnostics.push_back(MakeDiagnostic(
            DiagnosticReportWriterDiagnostics::OutputWriteFailed,
            "Diagnostics crash report JSON could not be written."));
        return result;
    }

    result.bytesWritten = payload.size();
    return result;
}

DiagnosticReportWriteResult DiagnosticReportWriter::WriteJsonToFile(
    const DiagnosticReport& report,
    const std::filesystem::path& outputPath)
{
    DiagnosticReportWriteResult result;

    const std::filesystem::path parentPath = outputPath.parent_path();
    if (!parentPath.empty())
    {
        std::error_code createError;
        std::filesystem::create_directories(parentPath, createError);
        if (createError)
        {
            result.diagnostics.push_back(MakeDiagnostic(
                DiagnosticReportWriterDiagnostics::OutputDirectoryFailed,
                "Diagnostics report output directory could not be created.",
                outputPath));
            return result;
        }
    }

    std::ofstream output(outputPath);
    if (!output.is_open())
    {
        result.diagnostics.push_back(MakeDiagnostic(
            DiagnosticReportWriterDiagnostics::OutputOpenFailed,
            "Diagnostics report output file could not be opened.",
            outputPath));
        return result;
    }

    result = WriteJson(report, output);
    if (!result.Succeeded())
    {
        for (auto& diagnostic : result.diagnostics)
        {
            diagnostic.outputPath = outputPath;
        }
    }
    return result;
}

DiagnosticReportWriteResult DiagnosticReportWriter::WriteJsonToFile(
    const CrashReport& report,
    const std::filesystem::path& outputPath)
{
    DiagnosticReportWriteResult result;

    const std::filesystem::path parentPath = outputPath.parent_path();
    if (!parentPath.empty())
    {
        std::error_code createError;
        std::filesystem::create_directories(parentPath, createError);
        if (createError)
        {
            result.diagnostics.push_back(MakeDiagnostic(
                DiagnosticReportWriterDiagnostics::OutputDirectoryFailed,
                "Diagnostics crash report output directory could not be created.",
                outputPath));
            return result;
        }
    }

    std::ofstream output(outputPath);
    if (!output.is_open())
    {
        result.diagnostics.push_back(MakeDiagnostic(
            DiagnosticReportWriterDiagnostics::OutputOpenFailed,
            "Diagnostics crash report output file could not be opened.",
            outputPath));
        return result;
    }

    result = WriteJson(report, output);
    if (!result.Succeeded())
    {
        for (auto& diagnostic : result.diagnostics)
        {
            diagnostic.outputPath = outputPath;
        }
    }
    return result;
}

} // namespace SagaEngine::Diagnostics
