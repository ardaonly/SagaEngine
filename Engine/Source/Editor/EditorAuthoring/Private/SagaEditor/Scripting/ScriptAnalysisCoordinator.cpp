// SPDX-License-Identifier: MPL-2.0

#include "SagaEditor/Scripting/ScriptAnalysisCoordinator.h"

#include "SagaEditor/Project/SagaProjectKitClient.h"
#include "SagaEditor/Workspace/EditorDocumentWorkspace.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <chrono>
#include <fstream>

namespace SagaEditor
{
namespace
{
using Json = nlohmann::json;

std::string HashFile(const std::filesystem::path& path, std::error_code& error)
{
    const auto bytes = ReadDocumentBytes(path, error);
    return error ? std::string{} : HashDocumentBytes(bytes);
}

bool IsContained(const std::filesystem::path& root, const std::filesystem::path& path)
{
    std::error_code error;
    const auto canonicalRoot = std::filesystem::weakly_canonical(root, error);
    if (error) return false;
    const auto canonicalPath = std::filesystem::weakly_canonical(path, error);
    if (error) return false;
    const auto relative = canonicalPath.lexically_relative(canonicalRoot);
    return !relative.empty() && !relative.is_absolute() &&
        *relative.begin() != std::filesystem::path("..");
}

bool ReadJson(const std::filesystem::path& path, Json& value)
{
    std::ifstream input(path, std::ios::binary);
    if (!input) return false;
    try { value = Json::parse(input); return value.is_object(); }
    catch (const Json::exception&) { return false; }
}

bool IsSha256(const std::string& value)
{
    return value.size() == 71 && value.rfind("sha256:", 0) == 0 &&
        std::all_of(value.begin() + 7, value.end(), [](unsigned char character)
        { return (character >= '0' && character <= '9') ||
                 (character >= 'a' && character <= 'f'); });
}

std::string OperationId(const EditorDocumentWorkspace& workspace)
{
    std::string material;
    if (const auto* project = workspace.Project()) material = project->projectId;
    for (const auto& source : workspace.Scripts())
    {
        material += source->Snapshot().canonicalSourcePath.generic_string();
        material += source->Snapshot().baseDiskHash;
    }
    material += std::to_string(
        std::chrono::steady_clock::now().time_since_epoch().count());
    return HashDocumentBytes(material).substr(7, 24);
}

std::string OperationId(const PlaySourceSnapshot& snapshot)
{
    std::string material = snapshot.projectId + snapshot.projectHash +
        snapshot.trackedLockHash;
    for (const auto& source : snapshot.sources)
        if (source.kind == "script") material += source.projectRelativePath.generic_string() + source.hash;
    material += std::to_string(
        std::chrono::steady_clock::now().time_since_epoch().count());
    return HashDocumentBytes(material).substr(7, 24);
}

DocumentDiagnostic Diagnostic(std::string code, std::string message,
                              std::filesystem::path path = {})
{
    DocumentDiagnostic result;
    result.code = std::move(code);
    result.message = std::move(message);
    result.path = std::move(path);
    return result;
}
}

ScriptAnalysisToolchain DiscoverScriptAnalysisToolchain()
{
    const auto discovered = ResolveEditorToolchain();
    ScriptAnalysisToolchain toolchain;
    toolchain.engineRoot = discovered.engineRoot;
    toolchain.sagaProjectKit = discovered.sagaProjectKitExecutable;
    toolchain.sagaPackager = discovered.sagaPackagerExecutable;
    toolchain.runtimeBridge = discovered.runtimeBridge;
    toolchain.sagaScript = discovered.sagaScriptExecutable;
    return toolchain;
}

ScriptAnalysisCoordinator::ScriptAnalysisCoordinator(ScriptAnalysisToolchain toolchain)
    : toolchain_(std::move(toolchain))
{
    if (toolchain_.engineRoot.empty()) toolchain_ = DiscoverScriptAnalysisToolchain();
}

ScriptAnalysisResult ScriptAnalysisCoordinator::Analyze(
    EditorDocumentWorkspace& workspace, const ScriptAnalysisRequest& request) const
{
    if (!workspace.IsOpen() || !workspace.Project() || !workspace.ProjectSource())
    {
        ScriptAnalysisResult result;
        result.diagnostics.push_back(Diagnostic("Script.Analysis.ProjectNotOpen",
            "A validated canonical project must be open before analysis."));
        return result;
    }
    const auto dirty = workspace.DirtyDocumentIds();
    if (!dirty.empty())
    {
        ScriptAnalysisResult result;
        result.diagnostics.push_back(Diagnostic("Script.Analysis.UnsavedDocuments",
            "Analysis consumes saved disk source. Save or discard dirty documents first."));
        return result;
    }
    const auto operationId = OperationId(workspace);
    const auto captured = CapturePlaySourceSnapshot(workspace, operationId);
    if (!captured.Succeeded())
    {
        ScriptAnalysisResult result;
        result.operationId = operationId;
        result.diagnostics = captured.diagnostics;
        return result;
    }
    return Analyze(*captured.snapshot, request);
}

ScriptAnalysisResult ScriptAnalysisCoordinator::Analyze(
    const PlaySourceSnapshot& snapshot, const ScriptAnalysisRequest& request) const
{
    ScriptAnalysisResult result;
    const std::filesystem::path projectRoot = snapshot.projectRoot;
    const std::filesystem::path projectPath = snapshot.projectPath;
    const auto buildRoot = projectRoot / "Build/Editor/Scripting";
    const auto packages = projectRoot / "Build/Packages";
    result.operationId = OperationId(snapshot);
    const auto transport = buildRoot / "Transport" / result.operationId;
    const auto packageReport = transport / "package-lock-verify.json";
    std::error_code error;
    std::filesystem::create_directories(transport, error);
    if (error)
    {
        result.diagnostics.push_back(Diagnostic("Script.Analysis.TransportCreateFailed",
            error.message(), transport));
        return result;
    }

    ToolProcessRequest packageRequest;
    packageRequest.executable = toolchain_.sagaPackager;
    packageRequest.arguments = {"lock-verify", "--project", projectPath.string(),
        "--engine-root", toolchain_.engineRoot.string(), "--lock",
        snapshot.trackedLockPath.string(), "--generated-out", packages.string(),
        "--runtime-bridge", toolchain_.runtimeBridge.string(), "--out", packageReport.string()};
    packageRequest.timeout = toolchain_.timeout;
    packageRequest.cancellationRequested = request.cancellationRequested;
    if (request.progress) request.progress("VerifyingPackages");
    result.packageProcess = ToolProcessRunner{}.Run(packageRequest);
    if (!result.packageProcess.Succeeded())
    {
        result.cancelled = result.packageProcess.classification ==
            ToolProcessExitClassification::Cancelled;
        result.diagnostics.push_back(Diagnostic("Script.Analysis.PackageVerifyFailed",
            result.packageProcess.standardError.empty() ?
                "SagaPackager rejected the tracked lock or selected package generation." :
                result.packageProcess.standardError, packageReport));
        return result;
    }

    Json packageStatus;
    if (!ReadJson(packageReport, packageStatus) ||
        packageStatus.value("status", std::string{}) != "Passed")
    {
        result.diagnostics.push_back(Diagnostic("Script.Analysis.PackageReportMalformed",
            "SagaPackager result is missing or did not pass.", packageReport));
        return result;
    }
    const auto currentPath = packages / "manifests/current.json";
    Json current;
    if (!ReadJson(currentPath, current) || current.value("schemaVersion", -1) != 1)
    {
        result.diagnostics.push_back(Diagnostic("Script.Analysis.PackageCurrentMalformed",
            "Selected package generation manifest is invalid.", currentPath));
        return result;
    }
    const auto generationId = current.value("generationId", std::string{});
    const auto generation = packages / "generations" / generationId;
    const auto generationManifestPath = packages /
        current.value("generationManifestPath", std::string{});
    if (generationId.size() != 64 ||
        !std::all_of(generationId.begin(), generationId.end(), [](unsigned char character)
        { return (character >= '0' && character <= '9') ||
                 (character >= 'a' && character <= 'f'); }) ||
        !IsSha256(current.value("generationHash", std::string{})) ||
        !IsContained(packages, generation) ||
        !IsContained(packages, generationManifestPath))
    {
        result.diagnostics.push_back(Diagnostic("Script.Analysis.PackageGenerationEscaped",
            "Selected package generation path is not contained in generated state.", currentPath));
        return result;
    }
    Json generationManifest;
    const auto expectedManifestHash = current.value("generationManifestHash", std::string{});
    const auto actualManifestHash = HashFile(generationManifestPath, error);
    if (error || !IsSha256(expectedManifestHash) || actualManifestHash != expectedManifestHash ||
        !ReadJson(generationManifestPath, generationManifest) ||
        generationManifest.value("schemaVersion", -1) != 1 ||
        generationManifest.value("generationId", std::string{}) != generationId ||
        generationManifest.value("generationHash", std::string{}) !=
            current.value("generationHash", std::string{}) ||
        !generationManifest.contains("files") || !generationManifest["files"].is_array())
    {
        result.diagnostics.push_back(Diagnostic("Script.Analysis.PackageGenerationMismatch",
            "Selected immutable package generation identity or manifest hash is invalid.",
            generationManifestPath));
        return result;
    }
    for (const auto& file : generationManifest["files"])
    {
        const auto path = generation / file.value("path", std::string{});
        const auto expected = file.value("hash", std::string{});
        if (!IsContained(generation, path) || !IsSha256(expected) ||
            HashFile(path, error) != expected || error)
        {
            result.diagnostics.push_back(Diagnostic("Script.Analysis.PackageFileMismatch",
                "A selected package generation file failed byte-hash verification.", path));
            return result;
        }
    }
    const auto referencePath = generation / "csharp_references_v1.json";
    const auto surfacePath = generation / "api_surfaces_v1.json";
    Json references;
    if (!ReadJson(referencePath, references) || !references.contains("references") ||
        !references["references"].is_array() || references["references"].size() != 1)
    {
        result.diagnostics.push_back(Diagnostic("Script.Analysis.ReferenceManifestMalformed",
            "Verified C# reference manifest must contain one RuntimeBridge reference.", referencePath));
        return result;
    }
    const auto& bridge = references["references"].front();
    const auto assembly = generation /
        bridge.value("resolvedAssemblyLogicalPath", std::string{});
    const auto descriptor = toolchain_.engineRoot / "Engine/Distribution" /
        bridge.value("distributionPath", std::string{});
    if (!IsContained(generation, assembly) ||
        !IsContained(toolchain_.engineRoot / "Engine/Distribution", descriptor))
    {
        result.diagnostics.push_back(Diagnostic("Script.Analysis.ReferencePathEscaped",
            "RuntimeBridge reference path escaped its verified owner.", referencePath));
        return result;
    }

    ScriptWorkerRequest workerRequest;
    workerRequest.operationId = result.operationId;
    workerRequest.projectId = snapshot.projectId;
    workerRequest.project.path = projectPath;
    workerRequest.project.hash = snapshot.projectHash;
    std::string revisionMaterial = workerRequest.project.hash;
    for (const auto& source : snapshot.sources)
    {
        if (source.kind != "script") continue;
        workerRequest.sources.push_back({source.projectRelativePath, source.hash});
        revisionMaterial += source.projectRelativePath.generic_string() + source.hash;
    }
    std::sort(workerRequest.sources.begin(), workerRequest.sources.end(),
        [](const auto& left, const auto& right)
        { return left.relativePath.generic_string() < right.relativePath.generic_string(); });
    workerRequest.sourceRevision = HashDocumentBytes(revisionMaterial);
    workerRequest.packageLock = {generation / "SagaPackage.lock",
        HashFile(generation / "SagaPackage.lock", error)};
    workerRequest.referenceManifest = {referencePath, HashFile(referencePath, error)};
    workerRequest.apiSurfaceManifest = {surfacePath, HashFile(surfacePath, error)};
    workerRequest.runtimeBridge.logicalReferenceId = bridge.value("artifactId", std::string{});
    workerRequest.runtimeBridge.descriptor = {descriptor,
        bridge.value("artifactHash", std::string{})};
    workerRequest.runtimeBridge.assembly = {assembly,
        bridge.value("resolvedAssemblyHash", std::string{})};
    workerRequest.runtimeBridge.targetFramework = bridge.value("targetFramework", std::string{});
    workerRequest.runtimeBridge.runtimeCompatibility =
        bridge.value("runtimeCompatibility", std::string{});
    workerRequest.runtimeBridge.trust = bridge.value("trust", std::string{});
    workerRequest.operations = {"analyze", "compile", "project-blocks"};
    workerRequest.outputRoot = buildRoot / "Artifacts";
    workerRequest.transportRoot = transport;
    workerRequest.cancellationRequested = request.cancellationRequested;
    workerRequest.progress = request.progress;
    if (error)
    {
        result.diagnostics.push_back(Diagnostic("Script.Analysis.VerifiedInputReadFailed",
            error.message(), generation));
        return result;
    }
    if (request.progress) request.progress("RunningSagaScript");
    result.worker = SagaScriptWorkerClient(toolchain_.sagaScript, toolchain_.timeout)
        .Run(workerRequest);
    result.cancelled = result.worker.cancelled;
    result.stale = result.worker.stale;
    result.diagnostics = result.worker.diagnostics;
    result.passed = result.worker.passed;
    const auto freshness = VerifyPlaySourceSnapshot(snapshot);
    if (result.passed && !freshness.empty())
    {
        result.passed = false;
        result.stale = true;
        result.diagnostics = freshness;
    }
    return result;
}
} // namespace SagaEditor
