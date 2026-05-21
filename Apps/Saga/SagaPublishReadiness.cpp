/// @file SagaPublishReadiness.cpp
/// @brief Product-owned publish readiness gate and report writer.

#include "SagaPublishReadiness.h"

#include "SagaProjectSystem.h"

#include <SagaEngine/Packages/PackageManifestLoader.hpp>

#include <nlohmann/json.hpp>

#include <fstream>
#include <sstream>
#include <utility>

namespace SagaProduct
{
namespace
{

namespace Diagnostics = SagaShared::Diagnostics;
namespace Publish = SagaShared::Publish;

constexpr const char* kClientPackageManifest =
    "Build/Manifests/package_manifest.client.json";
constexpr const char* kServerPackageManifest =
    "Build/Manifests/package_manifest.server.json";

[[nodiscard]] std::filesystem::path AbsoluteProjectRoot(
    const std::filesystem::path& projectRoot)
{
    if (projectRoot.empty())
    {
        return {};
    }
    return std::filesystem::absolute(projectRoot);
}

[[nodiscard]] std::string SeverityToString(
    Diagnostics::DiagnosticSeverity severity)
{
    switch (severity)
    {
        case Diagnostics::DiagnosticSeverity::Trace: return "trace";
        case Diagnostics::DiagnosticSeverity::Info: return "info";
        case Diagnostics::DiagnosticSeverity::Warning: return "warning";
        case Diagnostics::DiagnosticSeverity::Error: return "error";
        case Diagnostics::DiagnosticSeverity::Fatal: return "fatal";
        case Diagnostics::DiagnosticSeverity::BuildBlocking:
            return "build_blocking";
        case Diagnostics::DiagnosticSeverity::PublishBlocking:
            return "publish_blocking";
        case Diagnostics::DiagnosticSeverity::Security: return "security";
    }
    return "info";
}

[[nodiscard]] Diagnostics::DiagnosticPayload MakeDiagnostic(
    Diagnostics::DiagnosticSeverity severity,
    Diagnostics::DiagnosticCategory category,
    Diagnostics::DiagnosticSource source,
    std::string code,
    std::string title,
    std::string message,
    std::filesystem::path path = {})
{
    Diagnostics::DiagnosticPayload diagnostic;
    diagnostic.severity = severity;
    diagnostic.category = category;
    diagnostic.source = source;
    diagnostic.code.value = std::move(code);
    diagnostic.title = std::move(title);
    diagnostic.message = std::move(message);
    if (!path.empty())
    {
        diagnostic.location.sourcePath = path.string();
    }
    return diagnostic;
}

void AddBlocker(Publish::PublishReport& report,
                Publish::PublishBlockerKind kind,
                std::string message,
                std::string suggestedAction,
                Diagnostics::DiagnosticPayload diagnostic)
{
    Publish::PublishBlocker blocker;
    blocker.kind = kind;
    blocker.message = std::move(message);
    blocker.suggestedAction = std::move(suggestedAction);
    blocker.diagnostics.push_back(diagnostic);
    report.readiness.blockers.push_back(std::move(blocker));
    report.diagnosticSummary.Add(diagnostic);
}

[[nodiscard]] bool JsonBool(const nlohmann::json& json,
                            const char* key) noexcept
{
    return json.contains(key) && json[key].is_boolean() &&
        json[key].get<bool>();
}

[[nodiscard]] int JsonCount(const nlohmann::json& json,
                            const char* key) noexcept
{
    if (!json.contains(key) || !json[key].is_number_integer())
    {
        return 0;
    }
    return json[key].get<int>();
}

[[nodiscard]] const nlohmann::json& DiagnosticSummaryRoot(
    const nlohmann::json& root)
{
    if (root.contains("summary") && root["summary"].is_object())
    {
        return root["summary"];
    }
    return root;
}

[[nodiscard]] bool HasBlockingDiagnostics(const nlohmann::json& root)
{
    const nlohmann::json& summary = DiagnosticSummaryRoot(root);
    return JsonBool(summary, "hasBlockingDiagnostics") ||
        JsonCount(summary, "fatalCount") > 0 ||
        JsonCount(summary, "errorCount") > 0 ||
        JsonCount(summary, "securityCount") > 0 ||
        JsonCount(summary, "publishBlockingCount") > 0 ||
        JsonCount(summary, "buildBlockingCount") > 0;
}

[[nodiscard]] bool HasWarningDiagnostics(const nlohmann::json& root)
{
    const nlohmann::json& summary = DiagnosticSummaryRoot(root);
    return JsonCount(summary, "warningCount") > 0;
}

void ValidatePackageManifest(
    Publish::PublishReport& report,
    const std::filesystem::path& projectRoot,
    const std::filesystem::path& manifestPath,
    const char* expectedKind)
{
    SagaEngine::Packages::PackageManifestLoadOptions options;
    options.validateReferencedManifestFiles = true;
    options.packageBaseDirectory = projectRoot;

    const auto result =
        SagaEngine::Packages::PackageManifestLoader::LoadFromFile(
            manifestPath,
            options);
    if (!result.Succeeded())
    {
        for (const auto& error : result.errors)
        {
            auto diagnostic = MakeDiagnostic(
                Diagnostics::DiagnosticSeverity::PublishBlocking,
                Diagnostics::DiagnosticCategory::Package,
                Diagnostics::DiagnosticSource::Product,
                error.diagnosticId,
                "Package manifest is not publish-ready.",
                error.message,
                error.manifestPath);
            if (error.fieldName.has_value())
            {
                diagnostic.metadata["field"] = *error.fieldName;
            }
            if (error.referenceIndex.has_value())
            {
                diagnostic.metadata["referenceIndex"] =
                    std::to_string(*error.referenceIndex);
            }
            AddBlocker(
                report,
                Publish::PublishBlockerKind::PackageManifestInvalid,
                "Package manifest is invalid: " + manifestPath.string(),
                "Regenerate or repair the package manifest before publishing.",
                std::move(diagnostic));
        }
        return;
    }

    const std::string actualKind =
        std::string(SagaEngine::Packages::ToString(
            result.manifest.packageKind));
    if (actualKind != expectedKind)
    {
        auto diagnostic = MakeDiagnostic(
            Diagnostics::DiagnosticSeverity::PublishBlocking,
            Diagnostics::DiagnosticCategory::Package,
            Diagnostics::DiagnosticSource::Product,
            "Publish.Package.KindMismatch",
            "Package manifest kind does not match its publish slot.",
            "Expected package kind '" + std::string(expectedKind) +
                "' but found '" + actualKind + "'.",
            manifestPath);
        diagnostic.location.packageId = result.manifest.packageId;
        AddBlocker(
            report,
            Publish::PublishBlockerKind::PackageManifestInvalid,
            "Package manifest kind mismatch: " + manifestPath.string(),
            "Regenerate the package manifest for the expected package kind.",
            std::move(diagnostic));
    }
}

void ReadExternalDiagnostics(
    Publish::PublishReport& report,
    const SagaPublishDiagnosticsInput& input,
    bool& hasWarnings)
{
    std::ifstream stream(input.path);
    if (!stream.is_open())
    {
        auto diagnostic = MakeDiagnostic(
            Diagnostics::DiagnosticSeverity::PublishBlocking,
            Diagnostics::DiagnosticCategory::Publish,
            Diagnostics::DiagnosticSource::Product,
            "Publish.Diagnostics.Missing",
            "Publish diagnostics report is missing.",
            "Diagnostics report could not be opened: " + input.path.string(),
            input.path);
        diagnostic.metadata["key"] = input.key;
        AddBlocker(
            report,
            Publish::PublishBlockerKind::ToolchainInvalid,
            "Publish diagnostics report is missing: " + input.key,
            "Run the producing validation step before publish-check.",
            std::move(diagnostic));
        return;
    }

    nlohmann::json json;
    try
    {
        stream >> json;
    }
    catch (const nlohmann::json::exception& e)
    {
        auto diagnostic = MakeDiagnostic(
            Diagnostics::DiagnosticSeverity::PublishBlocking,
            Diagnostics::DiagnosticCategory::Publish,
            Diagnostics::DiagnosticSource::Product,
            "Publish.Diagnostics.InvalidJson",
            "Publish diagnostics report JSON is invalid.",
            std::string("Diagnostics report JSON parse failed: ") + e.what(),
            input.path);
        diagnostic.metadata["key"] = input.key;
        AddBlocker(
            report,
            Publish::PublishBlockerKind::ToolchainInvalid,
            "Publish diagnostics report is invalid: " + input.key,
            "Fix or regenerate the diagnostics report before publish-check.",
            std::move(diagnostic));
        return;
    }

    if (!json.is_object())
    {
        auto diagnostic = MakeDiagnostic(
            Diagnostics::DiagnosticSeverity::PublishBlocking,
            Diagnostics::DiagnosticCategory::Publish,
            Diagnostics::DiagnosticSource::Product,
            "Publish.Diagnostics.InvalidRoot",
            "Publish diagnostics report root is invalid.",
            "Diagnostics report root must be a JSON object.",
            input.path);
        diagnostic.metadata["key"] = input.key;
        AddBlocker(
            report,
            Publish::PublishBlockerKind::ToolchainInvalid,
            "Publish diagnostics report is invalid: " + input.key,
            "Fix or regenerate the diagnostics report before publish-check.",
            std::move(diagnostic));
        return;
    }

    if (HasBlockingDiagnostics(json))
    {
        auto diagnostic = MakeDiagnostic(
            Diagnostics::DiagnosticSeverity::PublishBlocking,
            Diagnostics::DiagnosticCategory::Publish,
            Diagnostics::DiagnosticSource::Product,
            "Publish.Diagnostics.Blocking",
            "Blocking diagnostics prevent publish.",
            "External diagnostics report contains blocking diagnostics.",
            input.path);
        diagnostic.metadata["key"] = input.key;
        AddBlocker(
            report,
            Publish::PublishBlockerKind::DiagnosticsFatal,
            "Blocking diagnostics prevent publish: " + input.key,
            "Resolve blocking diagnostics before publishing.",
            std::move(diagnostic));
    }
    else if (HasWarningDiagnostics(json))
    {
        hasWarnings = true;
        report.diagnosticSummary.Add(Diagnostics::DiagnosticSeverity::Warning);
    }
}

[[nodiscard]] nlohmann::json DiagnosticToJson(
    const Diagnostics::DiagnosticPayload& diagnostic)
{
    nlohmann::json json;
    json["severity"] = SeverityToString(diagnostic.severity);
    json["code"] = diagnostic.code.value;
    json["title"] = diagnostic.title;
    json["message"] = diagnostic.message;
    json["sourcePath"] = diagnostic.location.sourcePath;
    json["packageId"] = diagnostic.location.packageId;
    json["metadata"] = diagnostic.metadata;
    return json;
}

[[nodiscard]] nlohmann::json BlockerToJson(
    const Publish::PublishBlocker& blocker)
{
    nlohmann::json json;
    json["kind"] = ToString(blocker.kind);
    json["message"] = blocker.message;
    json["suggestedAction"] = blocker.suggestedAction;
    json["diagnostics"] = nlohmann::json::array();
    for (const auto& diagnostic : blocker.diagnostics)
    {
        json["diagnostics"].push_back(DiagnosticToJson(diagnostic));
    }
    json["metadata"] = blocker.metadata;
    return json;
}

[[nodiscard]] nlohmann::json SummaryToJson(
    const Diagnostics::DiagnosticSummary& summary)
{
    return {
        {"totalCount", summary.totalCount},
        {"warningCount", summary.warningCount},
        {"errorCount", summary.errorCount},
        {"fatalCount", summary.fatalCount},
        {"buildBlockingCount", summary.buildBlockingCount},
        {"publishBlockingCount", summary.publishBlockingCount},
        {"securityCount", summary.securityCount},
        {"highestSeverity", SeverityToString(summary.highestSeverity)}
    };
}

void WritePublishReport(const Publish::PublishReport& report,
                        const std::filesystem::path& reportPath)
{
    nlohmann::json json;
    json["schemaVersion"] = 1;
    json["reportId"] = report.reportId;
    json["buildId"] = report.buildId.value;
    json["packageId"] = report.packageId.value;
    json["status"] = ToString(report.readiness.status);
    json["diagnosticSummary"] = SummaryToJson(report.diagnosticSummary);
    json["metadata"] = report.metadata;
    json["blockers"] = nlohmann::json::array();
    for (const auto& blocker : report.readiness.blockers)
    {
        json["blockers"].push_back(BlockerToJson(blocker));
    }
    json["readiness"] = {
        {"status", ToString(report.readiness.status)},
        {"blockers", json["blockers"]}
    };

    if (!reportPath.parent_path().empty())
    {
        std::filesystem::create_directories(reportPath.parent_path());
    }
    std::ofstream output(reportPath, std::ios::trunc);
    output << json.dump(2) << '\n';
}

} // namespace

const char* ToString(SagaShared::Publish::PublishStatus status) noexcept
{
    switch (status)
    {
        case SagaShared::Publish::PublishStatus::Ready: return "ready";
        case SagaShared::Publish::PublishStatus::ReadyWithWarnings:
            return "ready_with_warnings";
        case SagaShared::Publish::PublishStatus::Blocked: return "blocked";
        case SagaShared::Publish::PublishStatus::Failed: return "failed";
        case SagaShared::Publish::PublishStatus::Unknown: return "unknown";
    }
    return "unknown";
}

const char* ToString(SagaShared::Publish::PublishBlockerKind kind) noexcept
{
    switch (kind)
    {
        case SagaShared::Publish::PublishBlockerKind::ProjectManifestInvalid:
            return "ProjectManifestInvalid";
        case SagaShared::Publish::PublishBlockerKind::PackageManifestInvalid:
            return "PackageManifestInvalid";
        case SagaShared::Publish::PublishBlockerKind::DiagnosticsFatal:
            return "DiagnosticsFatal";
        case SagaShared::Publish::PublishBlockerKind::ToolchainInvalid:
            return "ToolchainInvalid";
        case SagaShared::Publish::PublishBlockerKind::Unknown:
            return "Unknown";
        case SagaShared::Publish::PublishBlockerKind::SDECompileError:
            return "SDECompileError";
        case SagaShared::Publish::PublishBlockerKind::GraphValidationError:
            return "GraphValidationError";
        case SagaShared::Publish::PublishBlockerKind::AuthorityValidationError:
            return "AuthorityValidationError";
        case SagaShared::Publish::PublishBlockerKind::ScriptCompileError:
            return "ScriptCompileError";
        case SagaShared::Publish::PublishBlockerKind::AssetCookError:
            return "AssetCookError";
        case SagaShared::Publish::PublishBlockerKind::StaleArtifact:
            return "StaleArtifact";
        case SagaShared::Publish::PublishBlockerKind::CollaborationConflict:
            return "CollaborationConflict";
        case SagaShared::Publish::PublishBlockerKind::PermissionDenied:
            return "PermissionDenied";
        case SagaShared::Publish::PublishBlockerKind::RuntimeManifestInvalid:
            return "RuntimeManifestInvalid";
        case SagaShared::Publish::PublishBlockerKind::ServerPackageInvalid:
            return "ServerPackageInvalid";
    }
    return "Unknown";
}

std::filesystem::path SagaPublishReadinessService::DefaultReportPath(
    const std::filesystem::path& projectRoot)
{
    return AbsoluteProjectRoot(projectRoot) / "Build" / "Reports" /
        "publish_report.json";
}

std::vector<SagaPublishDiagnosticsInput>
SagaPublishReadinessService::ParseDiagnosticsInputs(
    const std::vector<std::string>& specs,
    std::string& error)
{
    std::vector<SagaPublishDiagnosticsInput> inputs;
    error.clear();

    for (const std::string& spec : specs)
    {
        const std::size_t separator = spec.find('=');
        if (separator == std::string::npos || separator == 0 ||
            separator + 1 >= spec.size())
        {
            error = "Publish diagnostics inputs must use key=path: " + spec;
            inputs.clear();
            return inputs;
        }

        SagaPublishDiagnosticsInput input;
        input.key = spec.substr(0, separator);
        input.path = spec.substr(separator + 1);
        inputs.push_back(std::move(input));
    }

    return inputs;
}

SagaPublishReadinessResult SagaPublishReadinessService::Check(
    const SagaPublishReadinessRequest& request) const
{
    SagaPublishReadinessResult result;
    const std::filesystem::path projectRoot =
        AbsoluteProjectRoot(request.projectRoot);
    result.reportPath = request.reportPath.empty()
        ? DefaultReportPath(projectRoot)
        : std::filesystem::absolute(request.reportPath);

    result.report.reportId = "publish-report:" + request.profile;
    result.report.packageId.value = "publish:" + request.profile;
    result.report.metadata["profile"] = request.profile;
    result.report.metadata["projectRoot"] = projectRoot.string();

    SagaProjectSystem projects(
        projectRoot / "Build" / "Reports" / "publish_recent_projects.json");
    const SagaProjectResult project = projects.OpenProject(projectRoot);
    if (!project.ok)
    {
        auto diagnostic = MakeDiagnostic(
            Diagnostics::DiagnosticSeverity::PublishBlocking,
            Diagnostics::DiagnosticCategory::Project,
            Diagnostics::DiagnosticSource::Product,
            "Publish.Project.ManifestInvalid",
            "Project manifest is not publish-ready.",
            project.error,
            projectRoot / "saga.project.json");
        AddBlocker(
            result.report,
            Publish::PublishBlockerKind::ProjectManifestInvalid,
            "Project manifest is invalid.",
            "Open or repair the Saga project before publishing.",
            std::move(diagnostic));
    }
    else
    {
        result.report.metadata["projectId"] = project.manifest.projectId;
        result.report.metadata["displayName"] = project.manifest.displayName;
    }

    ValidatePackageManifest(
        result.report,
        projectRoot,
        projectRoot / kClientPackageManifest,
        "client");
    ValidatePackageManifest(
        result.report,
        projectRoot,
        projectRoot / kServerPackageManifest,
        "server");

    bool hasWarnings = false;
    for (const SagaPublishDiagnosticsInput& input : request.diagnosticsInputs)
    {
        ReadExternalDiagnostics(result.report, input, hasWarnings);
    }

    if (!result.report.readiness.blockers.empty())
    {
        result.report.readiness.status = Publish::PublishStatus::Blocked;
    }
    else if (hasWarnings)
    {
        result.report.readiness.status =
            Publish::PublishStatus::ReadyWithWarnings;
    }
    else
    {
        result.report.readiness.status = Publish::PublishStatus::Ready;
    }

    WritePublishReport(result.report, result.reportPath);
    result.ok = result.report.readiness.IsReady();
    return result;
}

} // namespace SagaProduct
