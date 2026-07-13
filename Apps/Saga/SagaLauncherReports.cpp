/// @file SagaLauncherReports.cpp

#include "SagaLauncherReports.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <nlohmann/json.hpp>
#include <set>

namespace SagaProduct
{
namespace
{

using Json = nlohmann::json;

struct ReportSpec
{
    const char* type;
    const char* owner;
    SagaLauncherActionId action;
    const char* filename;
    int schemaVersion;
};

constexpr ReportSpec kLauncherReports[] = {
    {"project-validation",
     "sagaproject",
     SagaLauncherActionId::ValidateProject,
     "project-validation.json",
     1},
    {"runtime-smoke",
     "SagaRuntime",
     SagaLauncherActionId::RuntimeStarterArenaSmoke,
     "runtime-smoke.json",
     1},
    {"runtime-playable",
     "SagaRuntime",
     SagaLauncherActionId::RuntimeStarterArenaPlayable,
     "runtime-playable.json",
     1},
};

struct ProjectReportSpec
{
    const char* type;
    const char* filename;
    int schemaVersion;
};

constexpr ProjectReportSpec kProjectReports[] = {
    {"workflow-smoke", "workflow_smoke_report.json", 2},
    {"editor-inspection", "editor_shell_report.json", 2},
    {"package-staging", "package_stage_report.json", 1},
    {"publish-readiness", "publish_report.json", 1},
    {"sagascript-diagnostics", "sagascript_diagnostics.json", 1},
    {"local-workspace-transaction", "local_workspace_transaction_report.json", 1},
    {"local-collaboration-metadata", "local_collaboration_metadata_report.json", 1},
    {"visual-blocks-descriptor", "visual_blocks_descriptor.json", 1},
};

[[nodiscard]] std::filesystem::path LatestEvidenceRun(const std::filesystem::path& root)
{
    if (!std::filesystem::is_directory(root))
        return {};
    std::vector<std::filesystem::path> runs;
    std::error_code error;
    for (const auto& entry : std::filesystem::directory_iterator(root, error))
    {
        if (!entry.is_symlink() && entry.is_directory() &&
            entry.path().filename().string().rfind("run-", 0) == 0)
            runs.push_back(entry.path());
    }
    if (error || runs.empty())
        return {};
    return *std::max_element(runs.begin(), runs.end());
}

[[nodiscard]] SagaLauncherDiagnostic Diagnostic(const char* id,
                                                SagaLauncherDiagnosticSeverity severity,
                                                std::string message,
                                                const std::filesystem::path& path)
{
    SagaLauncherDiagnostic result;
    result.diagnosticId = id;
    result.severity = severity;
    result.message = std::move(message);
    result.path = path;
    return result;
}

[[nodiscard]] bool ReadJson(const std::filesystem::path& path, Json& json)
{
    std::ifstream input(path);
    if (!input.is_open())
        return false;
    try
    {
        input >> json;
    }
    catch (const Json::exception&)
    {
        return false;
    }
    return json.is_object();
}

[[nodiscard]] std::string RawStatus(const Json& json)
{
    for (const char* field : {"status", "gateStatus", "candidateStatus", "result"})
    {
        if (json.contains(field) && json[field].is_string())
            return json[field].get<std::string>();
    }
    if (json.contains("ready") && json["ready"].is_boolean())
        return json["ready"].get<bool>() ? "ready" : "not-ready";
    return "Unknown";
}

[[nodiscard]] std::string StringValue(const Json& json, const char* field)
{
    if (!json.contains(field) || !json[field].is_string())
        return {};
    return json[field].get<std::string>();
}

[[nodiscard]] SagaLauncherActionStatus MapStatus(const std::string& raw, const std::string& type)
{
    std::string lowered = raw;
    std::transform(lowered.begin(), lowered.end(), lowered.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    if (lowered.find("manualevidencepending") != std::string::npos ||
        lowered.find("limitation") != std::string::npos ||
        lowered.find("candidate") != std::string::npos ||
        lowered.find("staged") != std::string::npos ||
        (type == "publish-readiness" && lowered == "ready"))
        return SagaLauncherActionStatus::PassedWithLimitations;
    if (lowered == "passed" || lowered == "accepted" || lowered == "success")
        return SagaLauncherActionStatus::Passed;
    if (lowered == "failed" || lowered == "rejected" || lowered == "not-ready")
        return SagaLauncherActionStatus::Failed;
    if (lowered == "blocked")
        return SagaLauncherActionStatus::Blocked;
    if (lowered == "cancelled")
        return SagaLauncherActionStatus::Cancelled;
    return SagaLauncherActionStatus::Unknown;
}

void ReadStringArray(const Json& json, const char* field, std::vector<std::string>& output)
{
    if (!json.contains(field) || !json[field].is_array())
        return;
    for (const auto& value : json[field])
    {
        if (value.is_string())
            output.push_back(value.get<std::string>());
        else if (value.is_object() && value.contains("name") && value["name"].is_string())
            output.push_back(value["name"].get<std::string>());
    }
}

[[nodiscard]] SagaLauncherReportReference ReadReport(std::string type,
                                                     std::string owner,
                                                     std::filesystem::path path,
                                                     int expectedSchemaVersion)
{
    SagaLauncherReportReference report;
    report.reportType = std::move(type);
    report.owner = std::move(owner);
    report.path = std::move(path);
    report.exists = std::filesystem::is_regular_file(report.path);
    if (!report.exists)
        return report;

    Json json;
    if (!ReadJson(report.path, json))
    {
        report.rawStatus = "Invalid";
        report.mappedStatus = SagaLauncherActionStatus::Failed;
        report.diagnosticCount = 1;
        return report;
    }
    report.schemaVersion = json.contains("schemaVersion") &&
                                   json["schemaVersion"].is_number_integer()
                               ? json["schemaVersion"].get<int>()
                               : -1;
    report.rawStatus = RawStatus(json);
    if (report.schemaVersion != expectedSchemaVersion)
    {
        report.rawStatus = report.schemaVersion < 0
                               ? "InvalidSchema"
                               : "UnsupportedSchema(" + std::to_string(report.schemaVersion) + ")";
        report.mappedStatus = SagaLauncherActionStatus::Failed;
        report.diagnosticCount = 1;
        return report;
    }
    report.mappedStatus = MapStatus(report.rawStatus, report.reportType);
    if (json.contains("verified") && json["verified"].is_boolean())
        report.verified = json["verified"].get<bool>();
    ReadStringArray(json, "limitations", report.limitations);
    if (json.contains("diagnostics") && json["diagnostics"].is_array())
        report.diagnosticCount = json["diagnostics"].size();
    if (report.reportType == "publish-readiness" &&
        report.mappedStatus == SagaLauncherActionStatus::PassedWithLimitations)
        report.limitations.push_back(
            "Project publish-check evidence is not distribution or release readiness.");
    return report;
}

[[nodiscard]] std::filesystem::path FirstExisting(
    const std::vector<std::filesystem::path>& candidates)
{
    for (const auto& candidate : candidates)
        if (std::filesystem::is_regular_file(candidate))
            return candidate;
    return {};
}

} // namespace

SagaReportCatalog::SagaReportCatalog(std::filesystem::path launcherReportsRoot,
                                     std::filesystem::path evidenceRoot)
    : m_launcherReportsRoot(std::move(launcherReportsRoot)), m_evidenceRoot(std::move(evidenceRoot))
{}

std::vector<SagaLauncherReportReference> SagaReportCatalog::Read(
    const std::optional<SagaLauncherProjectSummary>& project) const
{
    std::vector<SagaLauncherReportReference> result;
    if (!project.has_value() || !project->valid)
        return result;
    for (const auto& spec : kLauncherReports)
    {
        result.push_back(ReadReport(spec.type,
                                    spec.owner,
                                    m_launcherReportsRoot / project->projectId /
                                        ToString(spec.action) / spec.filename,
                                    spec.schemaVersion));
    }
    const auto projectReportRoot = project->canonicalRoot / "Build" / "Reports";
    for (const auto& spec : kProjectReports)
        result.push_back(
            ReadReport(spec.type, "Saga", projectReportRoot / spec.filename, spec.schemaVersion));
    auto evidenceRun = LatestEvidenceRun(m_evidenceRoot / project->projectId);
    if (evidenceRun.empty())
        evidenceRun = m_evidenceRoot / project->projectId / "latest-not-configured";
    result.push_back(ReadReport(
        "first-playable-summary", "Saga", evidenceRun / "first_playable_summary.json", 1));
    result.push_back(
        ReadReport("first-playable-gate", "Saga", evidenceRun / "first_playable_gate.json", 1));
    result.push_back(ReadReport(
        "human-capture", "Saga", evidenceRun / "manual" / "human_capture_report.json", 1));
    return result;
}

SagaDistributionStatusReader::SagaDistributionStatusReader(std::filesystem::path productExecutable,
                                                           std::filesystem::path explicitReport)
    : m_productExecutable(std::move(productExecutable)), m_explicitReport(std::move(explicitReport))
{}

SagaLauncherDistributionSummary SagaDistributionStatusReader::Read() const
{
    SagaLauncherDistributionSummary result;
    std::vector<std::filesystem::path> candidates;
    if (!m_explicitReport.empty())
    {
        result.sourcePath = m_explicitReport;
        result.sourceKind = "explicit-cli";
        if (!std::filesystem::is_regular_file(result.sourcePath))
        {
            result.rawStatus = "MissingInput";
            result.mappedStatus = SagaLauncherActionStatus::MissingInput;
            result.diagnostics.push_back(
                Diagnostic(SagaLauncherReportDiagnostics::Invalid,
                           SagaLauncherDiagnosticSeverity::Error,
                           "Explicit launcher distribution report is not a regular file.",
                           result.sourcePath));
            return result;
        }
    }
    const auto productRoot = m_productExecutable.parent_path().parent_path();
    candidates.push_back(productRoot / "BUILD_INFO.json");
    candidates.push_back(productRoot / "reports" / "BUILD_INFO.json");
    if (result.sourcePath.empty())
        result.sourcePath = FirstExisting(candidates);
    if (result.sourcePath.empty())
    {
        result.rawStatus = "NotConfigured";
        result.mappedStatus = SagaLauncherActionStatus::NotConfigured;
        result.limitations.push_back("No installed or staged distribution report was found.");
        return result;
    }
    if (result.sourceKind.empty())
        result.sourceKind = "installed-or-staged";

    Json json;
    if (!ReadJson(result.sourcePath, json) || !json.contains("schemaVersion") ||
        !json["schemaVersion"].is_number_integer() || json["schemaVersion"].get<int>() != 2)
    {
        result.rawStatus = "Invalid";
        result.mappedStatus = SagaLauncherActionStatus::Failed;
        result.diagnostics.push_back(
            Diagnostic(SagaLauncherReportDiagnostics::Invalid,
                       SagaLauncherDiagnosticSeverity::Error,
                       "Distribution status requires a valid schema-2 JSON object.",
                       result.sourcePath));
        return result;
    }
    const Json* identity = &json;
    if (json.contains("buildInfo") && json["buildInfo"].is_object())
        identity = &json["buildInfo"];
    result.packageName = StringValue(json, "packageName");
    if (result.packageName.empty())
        result.packageName = StringValue(*identity, "packageName");
    if (result.packageName.empty())
        result.packageName = StringValue(*identity, "productName");
    result.version = StringValue(json, "version");
    if (result.version.empty())
        result.version = StringValue(*identity, "version");
    result.gitCommit = StringValue(json, "gitCommit");
    if (result.gitCommit.empty())
        result.gitCommit = StringValue(json, "sourceCommit");
    if (result.gitCommit.empty())
        result.gitCommit = StringValue(*identity, "gitCommit");
    if (result.gitCommit.empty())
        result.gitCommit = StringValue(*identity, "commit");
    result.platform = StringValue(json, "platform");
    if (result.platform.empty())
        result.platform = StringValue(*identity, "platform");
    result.rawStatus = RawStatus(json);
    result.mappedStatus = MapStatus(result.rawStatus, "distribution");
    result.verified = json.contains("verified") && json["verified"].is_boolean() &&
                      json["verified"].get<bool>();
    ReadStringArray(*identity, "includedApplications", result.includedApplications);
    if (result.includedApplications.empty())
        ReadStringArray(*identity, "includedTargets", result.includedApplications);
    ReadStringArray(*identity, "includedPublicTools", result.includedPublicTools);
    if (result.includedPublicTools.empty())
        ReadStringArray(*identity, "publicTools", result.includedPublicTools);
    ReadStringArray(*identity, "excludedRetiredTools", result.excludedRetiredTools);
    ReadStringArray(*identity, "excludedDevTools", result.excludedDevTools);
    ReadStringArray(json, "limitations", result.limitations);
    ReadStringArray(json, "knownLimitations", result.limitations);
    ReadStringArray(json, "finalAcceptanceGaps", result.limitations);
    result.archivePath = StringValue(json, "archivePath");
    if (result.archivePath.empty())
        result.archivePath = StringValue(json, "archive");
    if (result.archivePath.empty())
        result.archivePath = StringValue(*identity, "archiveTarget");
    result.checksumPath = StringValue(json, "checksumPath");
    if (result.checksumPath.empty())
        result.checksumPath = StringValue(json, "checksum");
    if (result.checksumPath.empty())
        result.checksumPath = StringValue(*identity, "checksumTarget");

    auto applications = result.includedApplications;
    auto tools = result.includedPublicTools;
    std::sort(applications.begin(), applications.end());
    std::sort(tools.begin(), tools.end());
    const bool hasIdentity = !applications.empty() || !tools.empty();
    if (hasIdentity &&
        (applications != std::vector<std::string>({"Saga", "SagaEditor", "SagaRuntime"}) ||
         tools != std::vector<std::string>({"sagapack", "sagaproject", "sagascript"})))
    {
        result.mappedStatus = SagaLauncherActionStatus::Failed;
        result.diagnostics.push_back(
            Diagnostic(SagaLauncherReportDiagnostics::IdentityMismatch,
                       SagaLauncherDiagnosticSeverity::Error,
                       "Distribution identity must contain exactly Saga, SagaEditor, SagaRuntime "
                       "and sagapack, sagaproject, sagascript.",
                       result.sourcePath));
    }
    else if (!hasIdentity)
    {
        result.mappedStatus = SagaLauncherActionStatus::PassedWithLimitations;
        result.limitations.push_back(
            "This schema-2 report does not carry the exact application/tool identity.");
    }
    else if (!result.verified && result.mappedStatus == SagaLauncherActionStatus::Passed)
    {
        result.mappedStatus = SagaLauncherActionStatus::PassedWithLimitations;
        result.limitations.push_back("Distribution report is not verified.");
    }
    return result;
}

} // namespace SagaProduct
