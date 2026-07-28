// SPDX-License-Identifier: MPL-2.0

#include "SagaEditor/Play/PlayPreparationPipeline.h"

#include "SagaEditor/Project/SagaProjectKitClient.h"
#include "SagaEditor/Processes/ToolProcessRunner.h"
#include "SagaEditor/Workspace/EditorDocumentWorkspace.h"
#include "SagaRuntime/Play/RuntimePlayContracts.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <fstream>

namespace SagaEditor
{
namespace
{
using Json = nlohmann::json;
namespace RuntimePlay = SagaRuntime::Play;

DocumentDiagnostic Diagnostic(std::string code, std::string message,
                              std::filesystem::path path = {})
{ return {std::move(code), std::move(message), std::move(path), 0, 0}; }

bool ReadJson(const std::filesystem::path& path, Json& value)
{
    std::ifstream input(path, std::ios::binary);
    if (!input) return false;
    try { value = Json::parse(input); return value.is_object(); }
    catch (const Json::exception&) { return false; }
}

std::string HashFile(const std::filesystem::path& path, std::error_code& error)
{
    const auto bytes = ReadDocumentBytes(path, error);
    return error ? std::string{} : HashDocumentBytes(bytes);
}

std::optional<Json> OneArtifact(const Json& manifest, std::string_view field,
                                std::string_view value)
{
    if (!manifest.contains("artifacts") || !manifest["artifacts"].is_array()) return std::nullopt;
    std::vector<Json> matches;
    for (const auto& artifact : manifest["artifacts"])
        if (artifact.value(std::string(field), std::string{}) == value) matches.push_back(artifact);
    return matches.size() == 1 ? std::optional<Json>(std::move(matches.front())) : std::nullopt;
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
} // namespace

PlayPreparationPipeline::PlayPreparationPipeline(ScriptAnalysisToolchain toolchain)
    : toolchain_(std::move(toolchain))
{
    if (toolchain_.engineRoot.empty()) toolchain_ = DiscoverScriptAnalysisToolchain();
}

PlayPreparationResult PlayPreparationPipeline::Prepare(
    EditorDocumentWorkspace& workspace, const PlayPreparationRequest& request) const
{
    const auto captured = CapturePlaySourceSnapshot(workspace, request.playSessionId);
    if (!captured.Succeeded())
    {
        PlayPreparationResult result;
        result.playSessionId = request.playSessionId;
        result.diagnostics = captured.diagnostics;
        result.stale = !result.diagnostics.empty() &&
            result.diagnostics.front().code == "Play.Prepare.StaleSource";
        return result;
    }
    return Prepare(*captured.snapshot, request);
}

PlayPreparationResult PlayPreparationPipeline::Prepare(
    const PlaySourceSnapshot& snapshot, const PlayPreparationRequest& request) const
{
    PlayPreparationResult result;
    result.playSessionId = request.playSessionId;
    if (request.playSessionId.empty() || request.playSessionId != snapshot.playSessionId ||
        request.runtimeExecutable.empty())
    {
        result.diagnostics.push_back(Diagnostic("Play.Prepare.InvalidRequest",
            "Open project, play-session identity, and SagaRuntime executable are required."));
        return result;
    }
    std::error_code error;
    const auto projectRoot = snapshot.projectRoot;
    result.buildRoot = projectRoot / "Build/Editor/Play" / request.playSessionId;
    std::filesystem::create_directories(result.buildRoot / "Transport", error);
    if (error)
    {
        result.diagnostics.push_back(Diagnostic("Play.Prepare.BuildRootFailed", error.message(), result.buildRoot));
        return result;
    }
    if (request.progress) request.progress("ValidateProject");
    result.projectValidation = SagaProjectKitClient(
        toolchain_.managedRuntime, toolchain_.timeout).Validate(snapshot.projectPath);
    if (!result.projectValidation.passed ||
        result.projectValidation.project.projectId != snapshot.projectId)
    {
        result.diagnostics = result.projectValidation.diagnostics;
        if (result.diagnostics.empty()) result.diagnostics.push_back(Diagnostic(
            "Play.Prepare.ProjectValidationFailed",
            "SagaProjectKit rejected the saved project identity."));
        return result;
    }
    if (request.progress) request.progress("AnalyzeAndCompileScripts");
    result.scriptAnalysis = ScriptAnalysisCoordinator(toolchain_).Analyze(snapshot,
        {request.cancellationRequested, request.progress});
    if (!result.scriptAnalysis.passed)
    {
        result.cancelled = result.scriptAnalysis.cancelled;
        result.stale = result.scriptAnalysis.stale;
        result.diagnostics = result.scriptAnalysis.diagnostics;
        if (result.diagnostics.empty()) result.diagnostics.push_back(Diagnostic(
            "Play.Prepare.ScriptFailed", "SagaScript did not publish a verified current artifact set."));
        return result;
    }
    const auto scriptingRoot = projectRoot / "Build/Editor/Scripting";
    const auto scriptRoot = scriptingRoot / "Artifacts";
    const auto scriptCurrentPath = scriptRoot / "manifests/current.json";
    const auto packageRoot = projectRoot / "Build/Packages";
    const auto packageCurrentPath = packageRoot / "manifests/current.json";
    const auto contentRoot = result.buildRoot / "Content";
    const auto contentReport = result.buildRoot / "Transport/content-result.json";
    if (request.progress) request.progress("CompileContent");
    ToolProcessRequest compile;
    compile.timeout = toolchain_.timeout;
    compile.cancellationRequested = request.cancellationRequested;
    result.contentProcess = ManagedToolRuntime(toolchain_.managedRuntime).Run(
        ManagedToolId::Packager,
        {"compile-content", "--project", snapshot.projectPath.string(),
         "--script-manifest", scriptCurrentPath.string(),
         "--out", contentRoot.string()},
        std::move(compile)).process;
    if (!result.contentProcess.Succeeded())
    {
        result.cancelled = result.contentProcess.classification ==
            ToolProcessExitClassification::Cancelled;
        result.diagnostics.push_back(Diagnostic("Play.Prepare.ContentFailed",
            result.contentProcess.standardError.empty() ?
                "SagaPackager did not publish runtime content." : result.contentProcess.standardError,
            contentReport));
        return result;
    }

    Json scriptCurrent, contentCurrent, packageCurrent;
    const auto contentCurrentPath = contentRoot / "manifests/current.json";
    if (!ReadJson(scriptCurrentPath, scriptCurrent) || !ReadJson(contentCurrentPath, contentCurrent) ||
        !ReadJson(packageCurrentPath, packageCurrent) ||
        scriptCurrent.value("schemaVersion", -1) != 1 ||
        contentCurrent.value("schemaVersion", -1) != 1 ||
        packageCurrent.value("schemaVersion", -1) != 1)
    {
        result.diagnostics.push_back(Diagnostic("Play.Prepare.CurrentManifestInvalid",
            "Package, script, and content current manifests must all use schema version 1."));
        return result;
    }
    const auto& projectId = snapshot.projectId;
    if (scriptCurrent.value("projectId", std::string{}) != projectId ||
        contentCurrent.value("projectId", std::string{}) != projectId ||
        packageCurrent.value("projectId", std::string{}) != projectId)
    {
        result.diagnostics.push_back(Diagnostic("Play.Prepare.ProjectIdentityMismatch",
            "Prepared generations disagree with the open project identity."));
        return result;
    }

    const auto sceneRecord = OneArtifact(contentCurrent, "artifactType", "runtime.scene");
    const auto inputRecord = OneArtifact(contentCurrent, "artifactType", "runtime.input-map");
    const auto uiRecord = OneArtifact(contentCurrent, "artifactType", "runtime.ui-set");
    const auto assemblyRecord = OneArtifact(scriptCurrent, "artifactId", "scripts.assembly");
    const auto runtimeConfigRecord = OneArtifact(scriptCurrent, "artifactId", "scripts.runtime-config");
    const auto behaviorRecord = OneArtifact(scriptCurrent, "artifactId", "scripts.behaviors");
    const auto actionRecord = OneArtifact(scriptCurrent, "artifactId", "scripts.named-actions");
    const auto capabilityRecord = OneArtifact(scriptCurrent, "artifactId", "scripts.capabilities");
    const auto assemblyManifestRecord = OneArtifact(scriptCurrent, "artifactId", "scripts.assembly-manifest");
    if (!sceneRecord || !inputRecord || !uiRecord || !assemblyRecord || !runtimeConfigRecord ||
        !behaviorRecord || !actionRecord ||
        !capabilityRecord || !assemblyManifestRecord)
    {
        result.diagnostics.push_back(Diagnostic("Play.Prepare.ArtifactSetIncomplete",
            "Exactly one runtime scene/input/UI set and verified script artifact set are required."));
        return result;
    }
    auto contentPath = [&](const Json& record)
    { return contentRoot / record.value("path", std::string{}); };
    auto scriptPath = [&](const Json& record)
    { return scriptRoot / record.value("objectPath", std::string{}); };
    const auto scenePath = contentPath(*sceneRecord);
    const auto inputPath = contentPath(*inputRecord);
    const auto uiPath = contentPath(*uiRecord);
    if (!IsContained(contentRoot, scenePath) || !IsContained(contentRoot, inputPath) ||
        !IsContained(contentRoot, uiPath) ||
        !RuntimePlay::LoadRuntimeSceneArtifact(scenePath).Succeeded() ||
        !RuntimePlay::LoadRuntimeInputMapArtifact(inputPath).Succeeded() ||
        !RuntimePlay::LoadRuntimeUiSetArtifact(uiPath).Succeeded())
    {
        result.diagnostics.push_back(Diagnostic("Play.Prepare.RuntimeArtifactInvalid",
            "Runtime scene, input, or UI artifact failed containment/schema/hash validation."));
        return result;
    }
    std::map<std::string, std::filesystem::path> scriptPaths;
    for (const auto* record : {&*assemblyRecord, &*runtimeConfigRecord,
                               &*behaviorRecord, &*actionRecord,
                               &*capabilityRecord, &*assemblyManifestRecord})
    {
        const auto path = scriptPath(*record);
        const auto expected = record->value("artifactHash", std::string{});
        if (!IsContained(scriptRoot, path) || !RuntimePlay::IsSha256(expected) ||
            HashFile(path, error) != expected || error)
        {
            result.diagnostics.push_back(Diagnostic("Play.Prepare.ScriptArtifactInvalid",
                "Script artifact failed containment or byte-hash validation.", path));
            return result;
        }
        scriptPaths[record->value("artifactId", std::string{})] = path;
    }
    Json assemblyManifest, behaviors, capabilities;
    if (!ReadJson(scriptPaths["scripts.assembly-manifest"], assemblyManifest) ||
        !ReadJson(scriptPaths["scripts.behaviors"], behaviors) ||
        !ReadJson(scriptPaths["scripts.capabilities"], capabilities) ||
        behaviors.value("schemaVersion", -1) != 2 ||
        !behaviors.contains("behaviors") || !behaviors["behaviors"].is_array() ||
        std::any_of(behaviors["behaviors"].begin(), behaviors["behaviors"].end(),
            [](const Json& behavior)
            {
                return !behavior.contains("stateAccesses") ||
                    !behavior["stateAccesses"].is_array();
            }) ||
        !capabilities.contains("capabilities") || !capabilities["capabilities"].is_array())
    {
        result.diagnostics.push_back(Diagnostic("Play.Prepare.ScriptMetadataInvalid",
            "Assembly, behavior schema v2 state policy, and capability manifests are required."));
        return result;
    }
    const auto generationId = packageCurrent.value("generationId", std::string{});
    const auto generationHash = packageCurrent.value("generationHash", std::string{});
    if (generationId.size() != 64 || !RuntimePlay::IsSha256(generationHash))
    {
        result.diagnostics.push_back(Diagnostic("Play.Prepare.PackageGenerationInvalid",
            "Selected package generation identity/hash is invalid."));
        return result;
    }
    const auto generation = packageRoot / "generations" / generationId;
    const auto generationManifestPath = packageRoot /
        packageCurrent.value("generationManifestPath", std::string{});
    const auto expectedManifestHash =
        packageCurrent.value("generationManifestHash", std::string{});
    if (!IsContained(packageRoot, generation) ||
        !IsContained(packageRoot, generationManifestPath) ||
        !RuntimePlay::IsSha256(expectedManifestHash))
    {
        result.diagnostics.push_back(Diagnostic("Play.Prepare.PackageGenerationInvalid",
            "Selected package generation paths or manifest hash are invalid."));
        return result;
    }
    const auto referencePath = generation / "csharp_references_v1.json";
    Json references;
    if (!ReadJson(referencePath, references) || !references.contains("references") ||
        !references["references"].is_array() || references["references"].size() != 1)
    {
        result.diagnostics.push_back(Diagnostic("Play.Prepare.RuntimeBridgeReferenceInvalid",
            "Selected package generation must pin one RuntimeBridge reference."));
        return result;
    }
    const auto& bridgeReference = references["references"].front();
    const auto bridgePath = generation /
        bridgeReference.value("resolvedAssemblyLogicalPath", std::string{});
    const auto bridgeHash = bridgeReference.value("resolvedAssemblyHash", std::string{});
    if (!IsContained(generation, bridgePath) || !RuntimePlay::IsSha256(bridgeHash) ||
        HashFile(bridgePath, error) != bridgeHash || error ||
        assemblyManifest.value("runtimeBridgeHash", std::string{}) != bridgeHash)
    {
        result.diagnostics.push_back(Diagnostic("Play.Prepare.RuntimeBridgeMismatch",
            "Selected RuntimeBridge binary does not match package/script metadata.", bridgePath));
        return result;
    }

    std::map<std::filesystem::path, std::string> publicationInputs;
    auto pinPublicationInput = [&](const std::filesystem::path& path,
                                   std::string expected = {})
    {
        std::error_code hashError;
        const auto actual = HashFile(path, hashError);
        if (hashError || !RuntimePlay::IsSha256(actual) ||
            (!expected.empty() && actual != expected)) return false;
        publicationInputs[path] = actual;
        return true;
    };
    const auto apiSurfacePath = generation / "api_surfaces_v1.json";
    if (!pinPublicationInput(packageCurrentPath) ||
        !pinPublicationInput(generationManifestPath, expectedManifestHash) ||
        !pinPublicationInput(generation / "SagaPackage.lock") ||
        !pinPublicationInput(referencePath) || !pinPublicationInput(apiSurfacePath) ||
        !pinPublicationInput(bridgePath, bridgeHash) ||
        !pinPublicationInput(contentCurrentPath) ||
        !pinPublicationInput(scenePath, sceneRecord->value("fileHash", std::string{})) ||
        !pinPublicationInput(inputPath, inputRecord->value("fileHash", std::string{})))
    {
        result.diagnostics.push_back(Diagnostic("Play.Prepare.PublicationInputInvalid",
            "A package, script, or content publication input changed before launch assembly."));
        return result;
    }
    for (const auto& [id, path] : scriptPaths)
    {
        const auto record = OneArtifact(scriptCurrent, "artifactId", id);
        if (!record || !pinPublicationInput(path,
                record->value("artifactHash", std::string{})))
        {
            result.diagnostics.push_back(Diagnostic("Play.Prepare.PublicationInputInvalid",
                "A verified SagaScript artifact changed before launch assembly.", path));
            return result;
        }
    }
    auto verifyPublicationInputs = [&]() -> std::vector<DocumentDiagnostic>
    {
        for (const auto& [path, expected] : publicationInputs)
        {
            std::error_code hashError;
            if (HashFile(path, hashError) != expected || hashError)
                return {Diagnostic("Play.Prepare.StaleDependency",
                    "A selected package, script, or content input changed during Play preparation.",
                    path)};
        }
        return {};
    };

    RuntimePlay::CanonicalRuntimeLaunchManifest launch;
    launch.schemaVersion = 2;
    launch.projectId = projectId;
    launch.projectHash = snapshot.projectHash;
    launch.playSessionId = request.playSessionId;
    launch.scene = {sceneRecord->value("artifactId", std::string{}),
        sceneRecord->value("fileHash", std::string{})};
    launch.input = {inputRecord->value("artifactId", std::string{}),
        inputRecord->value("fileHash", std::string{})};
    launch.uiSet = {uiRecord->value("artifactId", std::string{}),
        uiRecord->value("fileHash", std::string{})};
    launch.scriptAssembly = {"scripts.assembly", assemblyRecord->value("artifactHash", std::string{})};
    launch.scriptRuntimeConfig = {"scripts.runtime-config",
        runtimeConfigRecord->value("artifactHash", std::string{})};
    launch.scriptAssemblyManifest = {"scripts.assembly-manifest",
        assemblyManifestRecord->value("artifactHash", std::string{})};
    launch.scriptReferenceManifest = {"scripts.reference-manifest",
        HashFile(referencePath, error)};
    launch.behaviors = {"scripts.behaviors", behaviorRecord->value("artifactHash", std::string{})};
    launch.namedActions = {"scripts.named-actions", actionRecord->value("artifactHash", std::string{})};
    launch.capabilities = {"scripts.capabilities", capabilityRecord->value("artifactHash", std::string{})};
    launch.packageGeneration = {generationId, generationHash};
    launch.packageGraphHash = packageCurrent.value("graphHash", std::string{});
    launch.scriptSourceRevision = scriptCurrent.value("sourceRevision", std::string{});
    launch.scriptSourceSetHash = scriptCurrent.value("sourceSetHash", std::string{});
    launch.runtimeBridge = {bridgeReference.value("artifactId", std::string{}), bridgeHash,
        "NativeCallbackTable.v2+NamedAction.v1",
        bridgeReference.value("targetFramework", std::string{}),
        bridgeReference.value("runtimeCompatibility", std::string{}),
        bridgeReference.value("trust", std::string{})};
    for (const auto& capability : capabilities["capabilities"])
        if (capability.is_string()) launch.requiredRuntimeCapabilities.push_back(capability.get<std::string>());
    launch.toolVersions = {{"sagapack", contentCurrent.value("toolVersion", std::string{})},
                           {"sagascript", scriptCurrent.value("toolVersion", std::string{})},
                           {"compiler", scriptCurrent.value("compilerVersion", std::string{})}};
    const auto canonicalBytes = RuntimePlay::SerializeCanonical(launch);
    const auto canonicalValidation = RuntimePlay::ParseCanonicalManifest(canonicalBytes);
    if (!canonicalValidation.Succeeded())
    {
        result.diagnostics.push_back(Diagnostic("Play.Prepare.CanonicalManifestInvalid",
            "Generated runtime launch manifest failed its own strict contract."));
        return result;
    }
    result.canonicalManifestHash = canonicalValidation.hash;
    result.canonicalManifestPath = result.buildRoot / "launch/runtime-launch-v2.json";
    std::filesystem::create_directories(result.canonicalManifestPath.parent_path(), error);
    if (error)
    {
        result.diagnostics.push_back(Diagnostic("Play.Prepare.ManifestDirectoryFailed",
            error.message(), result.canonicalManifestPath.parent_path()));
        return result;
    }
    if (request.beforeLaunchPublication) request.beforeLaunchPublication();
    auto freshness = VerifyPlaySourceSnapshot(snapshot);
    if (!freshness.empty())
    {
        result.stale = true;
        result.diagnostics = std::move(freshness);
        return result;
    }
    freshness = verifyPublicationInputs();
    if (!freshness.empty())
    {
        result.stale = true;
        result.diagnostics = std::move(freshness);
        return result;
    }
    if (!AtomicWriteDocument(result.canonicalManifestPath, canonicalBytes,
                             request.playSessionId, error))
    {
        result.diagnostics.push_back(Diagnostic("Play.Prepare.ManifestWriteFailed",
            error.message(), result.canonicalManifestPath));
        return result;
    }

    RuntimePlay::ResolvedLocalLaunchEnvelope envelope;
    envelope.playSessionId = request.playSessionId;
    envelope.canonicalManifestPath = result.canonicalManifestPath;
    envelope.canonicalManifestHash = result.canonicalManifestHash;
    envelope.runtimeExecutable = request.runtimeExecutable;
    envelope.workingDirectory = result.buildRoot;
    envelope.resultPath = result.buildRoot / "runtime-result-v1.json";
    envelope.logPath = result.buildRoot / "SagaRuntime.log";
    envelope.stopPath = result.buildRoot / "stop-request-v1.json";
    envelope.startupReadyPath = result.buildRoot / "startup-ready-v1.json";
    envelope.environment = {{"SAGA_PLAY_SESSION_ID", request.playSessionId},
                            {"SAGA_RUNTIME_PROFILE", "Client"}};
    envelope.artifacts = {{launch.scene.logicalId, scenePath}, {launch.input.logicalId, inputPath},
        {launch.uiSet.logicalId, uiPath},
        {"scripts.assembly", scriptPaths["scripts.assembly"]},
        {"scripts.runtime-config", scriptPaths["scripts.runtime-config"]},
        {"scripts.assembly-manifest", scriptPaths["scripts.assembly-manifest"]},
        {"scripts.reference-manifest", referencePath},
        {"scripts.behaviors", scriptPaths["scripts.behaviors"]},
        {"scripts.named-actions", scriptPaths["scripts.named-actions"]},
        {"scripts.capabilities", scriptPaths["scripts.capabilities"]},
        {"packages.current", packageCurrentPath},
        {"packages.generation-manifest", generationManifestPath},
        {"packages.generation-lock", generation / "SagaPackage.lock"},
        {"packages.tracked-lock", snapshot.trackedLockPath},
        {launch.runtimeBridge.logicalReferenceId, bridgePath}};
    envelope.artifacts.emplace("content.current", contentCurrentPath);
    for (const auto& artifact : contentCurrent["artifacts"])
    {
        const auto id = artifact.value("artifactId", std::string{});
        const auto path = contentPath(artifact);
        const auto expected = artifact.value("fileHash", std::string{});
        if (id.empty() || !IsContained(contentRoot, path) ||
            !RuntimePlay::IsSha256(expected) || HashFile(path, error) != expected || error)
        {
            result.diagnostics.push_back(Diagnostic("Play.Prepare.ContentArtifactInvalid",
                "Cooked content failed identity, containment, or byte-hash validation.", path));
            return result;
        }
        envelope.artifacts.emplace(id, path);
    }
    const auto envelopeDiagnostics = RuntimePlay::ValidateLocalEnvelope(
        envelope, {projectRoot, toolchain_.engineRoot});
    if (!envelopeDiagnostics.empty())
    {
        for (const auto& diagnostic : envelopeDiagnostics)
            result.diagnostics.push_back(Diagnostic(diagnostic.code, diagnostic.message, diagnostic.subject));
        return result;
    }
    Json envelopeJson = {{"artifacts", Json::object()},
        {"canonicalManifestHash", envelope.canonicalManifestHash},
        {"canonicalManifestPath", envelope.canonicalManifestPath.string()},
        {"environment", envelope.environment}, {"logPath", envelope.logPath.string()},
        {"playSessionId", envelope.playSessionId}, {"resultPath", envelope.resultPath.string()},
        {"runtimeExecutable", envelope.runtimeExecutable.string()}, {"schemaVersion", 1},
        {"startupReadyPath", envelope.startupReadyPath.string()},
        {"stopPath", envelope.stopPath.string()}, {"workingDirectory", envelope.workingDirectory.string()}};
    for (const auto& [id, path] : envelope.artifacts) envelopeJson["artifacts"][id] = path.string();
    result.localEnvelopePath = result.buildRoot / "Transport/local-launch-envelope-v1.json";
    freshness = VerifyPlaySourceSnapshot(snapshot);
    if (!freshness.empty())
    {
        result.stale = true;
        result.diagnostics = std::move(freshness);
        return result;
    }
    freshness = verifyPublicationInputs();
    if (!freshness.empty())
    {
        result.stale = true;
        result.diagnostics = std::move(freshness);
        return result;
    }
    if (!AtomicWriteDocument(result.localEnvelopePath, envelopeJson.dump(2) + "\n",
                             request.playSessionId, error))
    {
        result.diagnostics.push_back(Diagnostic("Play.Prepare.EnvelopeWriteFailed",
            error.message(), result.localEnvelopePath));
        return result;
    }
    freshness = VerifyPlaySourceSnapshot(snapshot);
    if (!freshness.empty())
    {
        result.stale = true;
        result.diagnostics = std::move(freshness);
        return result;
    }
    freshness = verifyPublicationInputs();
    if (!freshness.empty())
    {
        result.stale = true;
        result.diagnostics = std::move(freshness);
        return result;
    }
    result.resultPath = envelope.resultPath;
    result.stopPath = envelope.stopPath;
    result.canonicalHashes = {{"scene", launch.scene.hash}, {"input", launch.input.hash},
        {"uiSet", launch.uiSet.hash},
        {"scriptAssembly", launch.scriptAssembly.hash},
        {"scriptRuntimeConfig", launch.scriptRuntimeConfig.hash},
        {"scriptAssemblyManifest", launch.scriptAssemblyManifest.hash},
        {"scriptReferenceManifest", launch.scriptReferenceManifest.hash},
        {"behaviors", launch.behaviors.hash},
        {"namedActions", launch.namedActions.hash}, {"capabilities", launch.capabilities.hash},
        {"packageGeneration", launch.packageGeneration.hash}, {"runtimeBridge", bridgeHash},
        {"launchManifest", result.canonicalManifestHash}};
    result.passed = true;
    if (request.progress) request.progress("Prepared");
    return result;
}
} // namespace SagaEditor
