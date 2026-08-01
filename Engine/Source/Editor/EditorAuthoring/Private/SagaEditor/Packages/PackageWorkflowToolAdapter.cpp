// SPDX-License-Identifier: MPL-2.0

#include "SagaEditor/Packages/PackageWorkflowToolAdapter.h"

#include "SagaEditor/Documents/SourceDocuments.h"
#include "SagaEditor/Play/PlaySourceSnapshot.h"
#include "SagaEditor/Project/SagaProjectKitClient.h"
#include "SagaEditor/Workspace/EditorDocumentWorkspace.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <fstream>

namespace SagaEditor
{
namespace
{
using Json = nlohmann::json;

bool ReadJson(const std::filesystem::path& path, Json& value,
              std::string* bytes = nullptr)
{
    std::error_code error;
    auto content = ReadDocumentBytes(path, error);
    if (error) return false;
    try { value = Json::parse(content); }
    catch (const Json::exception&) { return false; }
    if (!value.is_object()) return false;
    if (bytes) *bytes = std::move(content);
    return true;
}

std::string HashFile(const std::filesystem::path& path)
{
    std::error_code error;
    const auto bytes = ReadDocumentBytes(path, error);
    return error ? std::string{} : HashDocumentBytes(bytes);
}

PackageWorkflowResult Failure(std::string code, std::string message)
{
    return {PackageWorkflowStatus::Rejected, std::move(code),
        std::move(message), {}};
}

PackageCandidateResolution CandidateFailure(
    std::string code, std::string message)
{
    PackageCandidateResolution result;
    result.code = std::move(code);
    result.message = std::move(message);
    return result;
}

PackageUsageAnalysis UsageFailure(std::string code, std::string message)
{
    PackageUsageAnalysis result;
    result.code = std::move(code);
    result.message = std::move(message);
    return result;
}

ToolProcessRequest ProcessRequest(const PackageToolOperationContext& context,
                                  std::chrono::milliseconds timeout)
{
    ToolProcessRequest request;
    request.timeout = timeout;
    request.cancellationRequested = [cancellation = context.cancellation]
    {
        return cancellation && cancellation->load(std::memory_order_relaxed);
    };
    return request;
}

std::string ProcessFailure(const ManagedToolProcessResult& result)
{
    if (!result.preflightDiagnostics.empty())
        return result.preflightDiagnostics.front().message;
    if (!result.process.standardError.empty()) return result.process.standardError;
    if (!result.process.standardOutput.empty()) return result.process.standardOutput;
    return "The managed tool did not complete successfully.";
}
}

ManagedPackageWorkflowToolchain DiscoverManagedPackageWorkflowToolchain()
{
    const auto discovered = ResolveEditorToolchain();
    return {discovered.managedRuntime, discovered.engineRoot,
        discovered.runtimeBridge};
}

ManagedPackageWorkflowToolAdapter::ManagedPackageWorkflowToolAdapter(
    ManagedPackageWorkflowToolchain toolchain) : toolchain_(std::move(toolchain))
{
    if (toolchain_.engineRoot.empty())
        toolchain_ = DiscoverManagedPackageWorkflowToolchain();
}

std::pair<PackageWorkflowCapture, PackageWorkflowResult>
ManagedPackageWorkflowToolAdapter::Capture(
    const EditorDocumentWorkspace& workspace) const
{
    PackageWorkflowCapture capture;
    if (!workspace.IsOpen() || !workspace.Project() || !workspace.ProjectSource())
        return {capture, Failure("editor.packages.project-not-open",
            "A validated canonical project must be open.")};
    const auto snapshot = CapturePlaySourceSnapshot(workspace, "package-capture");
    if (!snapshot.Succeeded())
    {
        const auto message = snapshot.diagnostics.empty()
            ? "Saved project input capture failed."
            : snapshot.diagnostics.front().message;
        return {capture, Failure("editor.packages.capture-failed", message)};
    }
    const auto& saved = *snapshot.snapshot;
    capture.projectRoot = saved.projectRoot;
    capture.manifestPath = saved.projectPath;
    capture.lockPath = saved.trackedLockPath;
    std::error_code error;
    capture.manifestBytes = ReadDocumentBytes(capture.manifestPath, error);
    if (error) return {capture, Failure("editor.packages.manifest-read-failed",
        error.message())};
    capture.lockBytes = ReadDocumentBytes(capture.lockPath, error);
    if (error) return {capture, Failure("editor.packages.lock-read-failed",
        error.message())};
    capture.projectHash = saved.projectHash;
    capture.manifestHash = HashDocumentBytes(capture.manifestBytes);
    capture.lockHash = saved.trackedLockHash;
    Json sourceIdentity = Json::array();
    for (const auto& source : saved.sources)
    {
        if (source.kind != "script") continue;
        capture.savedSources.emplace_back(source.path, source.hash);
        sourceIdentity.push_back({{"path", source.path.generic_string()},
            {"hash", source.hash}});
    }
    std::sort(capture.savedSources.begin(), capture.savedSources.end());
    capture.sourceInventoryHash = HashDocumentBytes(sourceIdentity.dump(2) + "\n");

    const auto packages = capture.projectRoot / "Build/Packages";
    Json current;
    if (!ReadJson(packages / "manifests/current.json", current) ||
        current.value("schemaVersion", -1) != 1)
        return {capture, Failure("editor.packages.generation-invalid",
            "The selected package generation manifest is missing or invalid.")};
    capture.generationId = current.value("generationId", std::string{});
    capture.generationHash = current.value("generationHash", std::string{});
    capture.generationManifestHash = current.value(
        "generationManifestHash", std::string{});
    capture.graphHash = current.value("graphHash", std::string{});
    const auto generation = packages / "generations" / capture.generationId;
    Json lock;
    const auto generationManifest = generation / "generation.json";
    capture.derivedStateCurrent = ReadJson(capture.lockPath, lock) &&
        current.value("projectHash", std::string{}) == capture.projectHash &&
        lock.value("projectHash", std::string{}) == capture.projectHash &&
        lock.value("graphHash", std::string{}) == capture.graphHash &&
        HashFile(generationManifest) == capture.generationManifestHash;
    const auto ownership = generation / "api_ownership_v1.json";
    capture.ownershipCatalogHash = HashFile(ownership);
    Json referenceManifest;
    if (capture.ownershipCatalogHash.empty() ||
        !ReadJson(generation / "csharp_references_v1.json", referenceManifest))
        return {capture, Failure("editor.packages.generation-invalid",
            "Package ownership or reference metadata is unavailable.")};
    Json references = Json::array();
    for (const auto& item : referenceManifest.value("references", Json::array()))
    {
        const auto path = generation /
            item.value("resolvedAssemblyLogicalPath", std::string{});
        const auto hash = item.value("resolvedAssemblyHash", std::string{});
        if (hash.empty() || HashFile(path) != hash)
            return {capture, Failure("editor.packages.reference-stale",
                "A package reference failed exact hash verification.")};
        capture.references.emplace_back(path, hash);
    }
    std::sort(capture.references.begin(), capture.references.end());
    for (const auto& [path, hash] : capture.references)
        references.push_back({{"path", path.generic_string()}, {"hash", hash}});
    capture.referenceInventoryHash = HashDocumentBytes(references.dump(2) + "\n");
    return {capture, {PackageWorkflowStatus::Prepared, {}, {}, {}}};
}

PackageInspectionResult ManagedPackageWorkflowToolAdapter::RunPackager(
    std::vector<std::string> arguments, const std::filesystem::path& report,
    const PackageToolOperationContext& context,
    std::string_view failureCode) const
{
    const auto process = ManagedToolRuntime(toolchain_.managedRuntime).Run(
        ManagedToolId::Packager, std::move(arguments),
        ProcessRequest(context, toolchain_.timeout));
    if (!process.Succeeded())
        return {false, std::string(failureCode), ProcessFailure(process), {}};
    Json payload;
    std::string bytes;
    if (!ReadJson(report, payload, &bytes) ||
        payload.value("status", std::string{}) != "Passed")
        return {false, std::string(failureCode),
            "SagaPackager returned an invalid or failed report.", {}};
    return {true, {}, {}, std::move(bytes)};
}

PackageCandidateResolution ManagedPackageWorkflowToolAdapter::ResolveCandidate(
    const PackageWorkflowCapture& capture,
    const std::filesystem::path& stagedManifest,
    const std::filesystem::path& stagingRoot,
    const PackageToolOperationContext& context) const
{
    if (context.progress) context.progress("Resolving");
    const auto candidateLock = stagingRoot / "candidate.lock";
    const auto generated = stagingRoot / "generated";
    const auto resolveReport = stagingRoot / "resolve-candidate.json";
    auto resolved = RunPackager({"resolve-candidate", "--project",
        stagedManifest.string(), "--project-root", capture.projectRoot.string(),
        "--engine-root", toolchain_.engineRoot.string(), "--lock",
        candidateLock.string(), "--generated-out", generated.string(),
        "--runtime-bridge", toolchain_.runtimeBridge.string(), "--out",
        resolveReport.string()}, resolveReport, context,
        "editor.packages.resolver-failed");
    if (!resolved.passed)
        return CandidateFailure(resolved.code, resolved.message);
    const auto inspectionReport = stagingRoot / "candidate-inspection.json";
    auto inspected = RunPackager({"inspect", "--project", stagedManifest.string(),
        "--project-root", capture.projectRoot.string(), "--engine-root",
        toolchain_.engineRoot.string(), "--lock", candidateLock.string(),
        "--generated-out", generated.string(), "--runtime-bridge",
        toolchain_.runtimeBridge.string(), "--out", inspectionReport.string()},
        inspectionReport, context, "editor.packages.candidate-inspection-failed");
    if (!inspected.passed)
        return CandidateFailure(inspected.code, inspected.message);
    Json report;
    if (!ReadJson(resolveReport, report))
        return CandidateFailure("editor.packages.resolver-report-invalid",
            "The candidate resolver report could not be read.");
    std::error_code error;
    const auto lockBytes = ReadDocumentBytes(candidateLock, error);
    if (error) return CandidateFailure(
        "editor.packages.candidate-lock-invalid", error.message());
    return {true, {}, {}, lockBytes, inspected.inspectionBytes,
        report.value("graphHash", std::string{})};
}

PackageUsageAnalysis ManagedPackageWorkflowToolAdapter::AnalyzeUsage(
    const PackageWorkflowCapture& capture, std::string_view packageId,
    std::string_view module, const std::filesystem::path& stagingRoot,
    const PackageToolOperationContext& context) const
{
    PackageUsageAnalysis result;
    if (context.progress) context.progress("Analyzing");
    std::error_code stagingError;
    std::filesystem::create_directories(stagingRoot, stagingError);
    if (stagingError)
        return UsageFailure("editor.packages.usage-request-write-failed",
            stagingError.message());
    Json request = {{"schemaVersion", 1},
        {"projectId", ""}, {"projectHash", capture.projectHash},
        {"lockHash", capture.lockHash}, {"graphHash", capture.graphHash},
        {"generationId", capture.generationId},
        {"generationHash", capture.generationHash},
        {"generationManifestHash", capture.generationManifestHash},
        {"referenceHash", capture.referenceInventoryHash},
        {"ownershipCatalogPath", (capture.projectRoot / "Build/Packages/generations" /
            capture.generationId / "api_ownership_v1.json").generic_string()},
        {"ownershipCatalogHash", capture.ownershipCatalogHash},
        {"packageId", packageId}, {"module", module}};
    Json manifest;
    if (!ReadJson(capture.manifestPath, manifest))
        return UsageFailure("editor.packages.usage-request-invalid",
            "The canonical project identity could not be read.");
    request["projectId"] = manifest.value("projectId", std::string{});
    request["sources"] = Json::array();
    for (const auto& [path, hash] : capture.savedSources)
        request["sources"].push_back({{"path", path.generic_string()}, {"hash", hash}});
    request["references"] = Json::array();
    for (const auto& [path, hash] : capture.references)
        request["references"].push_back({{"path", path.generic_string()}, {"hash", hash}});
    const auto requestPath = stagingRoot / "module-usage-request.json";
    const auto reportPath = stagingRoot / "module-usage-report.json";
    {
        std::ofstream output(requestPath, std::ios::binary | std::ios::trunc);
        output << request.dump(2) << '\n';
        if (!output) return UsageFailure(
            "editor.packages.usage-request-write-failed",
            "The module usage request could not be written.");
    }
    const auto process = ManagedToolRuntime(toolchain_.managedRuntime).Run(
        ManagedToolId::Script, {"module-usage", "--request",
            requestPath.string(), "--out", reportPath.string()},
        ProcessRequest(context, toolchain_.timeout));
    if (!process.Succeeded())
        return UsageFailure(
            "editor.packages.usage-failed", ProcessFailure(process));
    Json report;
    if (!ReadJson(reportPath, report) ||
        report.value("status", std::string{}) != "Passed")
        return UsageFailure("editor.packages.usage-report-invalid",
            "SagaScript returned an invalid module usage report.");
    result.passed = true;
    result.sourceInventoryHash = capture.sourceInventoryHash;
    result.generationId = report.value("generationId", std::string{});
    result.generationHash = report.value("generationHash", std::string{});
    result.graphHash = report.value("graphHash", std::string{});
    result.ownershipCatalogHash = report.value("ownershipCatalogHash", std::string{});
    result.projectHash = report.value("projectHash", std::string{});
    result.lockHash = report.value("lockHash", std::string{});
    result.generationManifestHash = report.value(
        "generationManifestHash", std::string{});
    result.referenceInventoryHash = report.value("referenceHash", std::string{});
    for (const auto& item : report.value("usages", Json::array()))
    {
        PackageUsageLocation location;
        location.sourcePath = item.value("sourcePath", std::string{});
        location.startByte = item.value("startByte", 0ull);
        location.endByte = item.value("endByte", 0ull);
        location.startLine = item.value("startLine", 0);
        location.startColumn = item.value("startColumn", 0);
        location.endLine = item.value("endLine", 0);
        location.endColumn = item.value("endColumn", 0);
        location.containingType = item.value("containingType", std::string{});
        location.containingBehavior = item.value("containingBehavior", std::string{});
        location.documentationId = item.value("documentationId", std::string{});
        location.apiId = item.value("apiId", std::string{});
        location.surface = item.value("surface", std::string{});
        location.module = item.value("module", std::string{});
        location.packageId = item.value("packageId", std::string{});
        result.locations.push_back(std::move(location));
    }
    return result;
}

PackageInspectionResult ManagedPackageWorkflowToolAdapter::RefreshGeneration(
    const PackageWorkflowCapture& capture,
    const PackageToolOperationContext& context) const
{
    if (context.progress) context.progress("Refreshing");
    const auto packages = capture.projectRoot / "Build/Packages";
    const auto transport = capture.projectRoot /
        "Build/Editor/PackageWorkflow/generation-refresh.json";
    std::error_code error;
    std::filesystem::create_directories(transport.parent_path(), error);
    if (error) return {false, "editor.packages.refresh-output-failed",
        error.message(), {}};
    auto refreshed = RunPackager({"generation-refresh", "--project",
        capture.manifestPath.string(), "--engine-root", toolchain_.engineRoot.string(),
        "--lock", capture.lockPath.string(), "--generated-out", packages.string(),
        "--runtime-bridge", toolchain_.runtimeBridge.string(), "--out",
        transport.string()}, transport, context,
        "editor.packages.generation-refresh-failed");
    if (!refreshed.passed) return refreshed;
    return InspectCanonical(capture, context);
}

PackageInspectionResult ManagedPackageWorkflowToolAdapter::InspectCanonical(
    const PackageWorkflowCapture& capture,
    const PackageToolOperationContext& context) const
{
    if (context.progress) context.progress("Inspecting");
    const auto output = capture.projectRoot /
        "Build/Editor/PackageWorkflow/package-inspection.json";
    std::error_code error;
    std::filesystem::create_directories(output.parent_path(), error);
    if (error) return {false, "editor.packages.inspection-output-failed",
        error.message(), {}};
    return RunPackager({"inspect", "--project", capture.manifestPath.string(),
        "--project-root", capture.projectRoot.string(), "--engine-root",
        toolchain_.engineRoot.string(), "--lock", capture.lockPath.string(),
        "--generated-out", (capture.projectRoot / "Build/Packages").string(),
        "--runtime-bridge", toolchain_.runtimeBridge.string(), "--out",
        output.string()}, output, context, "editor.packages.inspection-failed");
}
}
