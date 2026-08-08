// SPDX-License-Identifier: MPL-2.0

#include "SagaEditor/Standalone/StandaloneBuildCoordinator.h"

#include "SagaEditor/Play/PlayPreparationPipeline.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <chrono>
#include <fstream>
#include <set>

namespace SagaEditor
{
namespace
{
using Json = nlohmann::ordered_json;

DocumentDiagnostic Diagnostic(std::string code, std::string message,
                              std::filesystem::path path = {})
{ return {std::move(code), std::move(message), std::move(path), 0, 0}; }

std::string HashFile(const std::filesystem::path& path, std::error_code& error)
{
    const auto bytes = ReadDocumentBytes(path, error);
    return error ? std::string{} : HashDocumentBytes(bytes);
}

std::string ContentId(std::string_view bytes)
{
    auto hash = HashDocumentBytes(std::string(bytes));
    constexpr std::string_view prefix = "sha256:";
    if (hash.starts_with(prefix)) hash.erase(0, prefix.size());
    return hash;
}

bool IsHash(std::string_view value)
{
    if (value.starts_with("sha256:")) value.remove_prefix(7);
    return value.size() == 64 && std::all_of(value.begin(), value.end(), [](char character)
    { return (character >= '0' && character <= '9') ||
             (character >= 'a' && character <= 'f'); });
}

bool ReadJson(const std::filesystem::path& path, nlohmann::json& value)
{
    std::ifstream input(path, std::ios::binary);
    if (!input) return false;
    try { value = nlohmann::json::parse(input); return value.is_object(); }
    catch (const nlohmann::json::exception&) { return false; }
}

bool Contained(const std::filesystem::path& root, const std::filesystem::path& path)
{
    std::error_code error;
    const auto canonicalRoot = std::filesystem::weakly_canonical(root, error);
    if (error) return false;
    const auto canonicalPath = std::filesystem::weakly_canonical(path, error);
    if (error) return false;
    const auto relative = canonicalPath.lexically_relative(canonicalRoot);
    return !relative.empty() && !relative.is_absolute() && *relative.begin() != "..";
}

void Report(const StandaloneBuildRequest& request, StandaloneBuildState state,
            unsigned completed, unsigned total, std::string message)
{
    if (request.progress)
        request.progress({state, completed, total, std::move(message)});
}

bool SafeOutput(const StandaloneBuildRequest& request,
                std::vector<DocumentDiagnostic>& diagnostics)
{
    if (request.outputDirectory.empty() ||
        request.outputDirectory == request.outputDirectory.root_path())
    {
        diagnostics.push_back(Diagnostic("Build.Output.UnsafeRoot",
            "Standalone output must be a specific non-root directory."));
        return false;
    }
    std::error_code error;
    const auto parent = std::filesystem::weakly_canonical(
        request.outputDirectory.parent_path(), error);
    if (error) return false;
    const auto output = parent / request.outputDirectory.filename();
    const auto project = std::filesystem::weakly_canonical(
        request.source.projectRoot, error);
    const auto repository = std::filesystem::weakly_canonical(
        request.toolchain.engineRoot.empty() ? request.source.projectRoot.parent_path()
                                             : request.toolchain.engineRoot, error);
    if (error || output == project || output == repository || Contained(output, project) ||
        Contained(output, request.source.projectPath))
    {
        diagnostics.push_back(Diagnostic("Build.Output.OverlapsSource",
            "Standalone output may not be the repository, project, or a source path.", output));
        return false;
    }
    for (auto cursor = output; !cursor.empty(); cursor = cursor.parent_path())
    {
        if (std::filesystem::is_symlink(cursor, error))
        {
            diagnostics.push_back(Diagnostic("Build.Output.SymlinkEscape",
                "Standalone output ancestors may not be symbolic links.", cursor));
            return false;
        }
        if (cursor == cursor.root_path()) break;
    }
    return true;
}

Json SnapshotJson(const BuildInputSnapshot& snapshot)
{
    Json files = Json::array();
    for (const auto& file : snapshot.files)
        files.push_back({{"hash", file.hash},
            {"path", file.snapshotPath.generic_string()}, {"size", file.size}});
    return {{"configuration", snapshot.configuration}, {"files", std::move(files)},
        {"lockHash", snapshot.lockHash},
        {"packageGenerationHash", snapshot.packageGenerationHash},
        {"packageGenerationId", snapshot.packageGenerationId},
        {"packageGraphHash", snapshot.packageGraphHash},
        {"profileHash", snapshot.profileHash}, {"profileId", snapshot.profileId},
        {"projectHash", snapshot.projectHash}, {"projectId", snapshot.projectId},
        {"runtimeHash", snapshot.runtimeHash},
        {"runtimeIdentifier", snapshot.runtimeIdentifier}, {"schemaVersion", 1},
        {"target", snapshot.target}, {"toolHashes", snapshot.toolHashes}};
}

bool AddFile(BuildInputSnapshot& snapshot, const std::filesystem::path& source,
             const std::filesystem::path& relative,
             std::set<std::filesystem::path>& seen,
             std::vector<DocumentDiagnostic>& diagnostics)
{
    const auto normalized = relative.lexically_normal();
    if (normalized.empty() || normalized.is_absolute() || *normalized.begin() == "..")
    {
        diagnostics.push_back(Diagnostic("Build.Snapshot.InputEscape",
            "A build input escaped the project snapshot.", source));
        return false;
    }
    if (!seen.insert(normalized).second) return true;
    std::error_code error;
    const auto size = std::filesystem::file_size(source, error);
    const auto hash = error ? std::string{} : HashFile(source, error);
    if (error || hash.empty())
    {
        diagnostics.push_back(Diagnostic("Build.Snapshot.InputUnreadable",
            "A required standalone input could not be read.", source));
        return false;
    }
    snapshot.files.push_back({source, normalized, hash, size});
    return true;
}

bool CopyTree(const std::filesystem::path& source,
              const std::filesystem::path& destination, std::error_code& error)
{
    std::filesystem::create_directories(destination, error);
    if (error) return false;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(source, error))
    {
        if (error || entry.is_symlink()) return false;
        const auto relative = std::filesystem::relative(entry.path(), source, error);
        if (error) return false;
        const auto relativeText = relative.generic_string();
        if (relativeText == "Build/Editor/Scripting/Transport" ||
            relativeText.starts_with("Build/Editor/Scripting/Transport/"))
            continue;
        const auto target = destination / relative;
        if (entry.is_directory()) std::filesystem::create_directories(target, error);
        else if (entry.is_regular_file())
        {
            std::filesystem::create_directories(target.parent_path(), error);
            if (!error) std::filesystem::copy_file(entry.path(), target,
                std::filesystem::copy_options::overwrite_existing, error);
        }
        if (error) return false;
    }
    return true;
}

void RebaseJsonPaths(nlohmann::json& value, const std::string& oldRoot,
                     const std::string& newRoot,
                     const std::string& oldRuntime,
                     const std::string& newRuntime)
{
    if (value.is_object())
        for (auto& [key, child] : value.items())
        { (void)key; RebaseJsonPaths(child, oldRoot, newRoot, oldRuntime, newRuntime); }
    else if (value.is_array())
        for (auto& child : value)
            RebaseJsonPaths(child, oldRoot, newRoot, oldRuntime, newRuntime);
    else if (value.is_string())
    {
        auto text = value.get<std::string>();
        if (text == oldRuntime) text = newRuntime;
        else if (text.starts_with(oldRoot)) text.replace(0, oldRoot.size(), newRoot);
        value = std::move(text);
    }
}

StandaloneBuildExecution DefaultExecute(const PlaySourceSnapshot& source,
    const std::filesystem::path&, const StandaloneBuildRequest& request)
{
    const auto prepared = PlayPreparationPipeline(request.toolchain).Prepare(source,
        {source.playSessionId, request.runtimeExecutable,
         request.cancellationRequested,
         [&](std::string_view value)
         { Report(request, StandaloneBuildState::Building, 0, 0, std::string(value)); }, {}});
    return {prepared.passed, prepared.cancelled, prepared.buildRoot,
        prepared.canonicalManifestPath, prepared.localEnvelopePath,
        prepared.canonicalHashes, prepared.diagnostics};
}
} // namespace

std::optional<BuildInputSnapshot> StandaloneBuildCoordinator::Capture(
    const StandaloneBuildRequest& request,
    std::vector<DocumentDiagnostic>& diagnostics)
{
    if (request.profileId.empty() || !IsHash(request.profileHash) ||
        request.runtimeExecutable.empty() || request.runtimeIdentifier.empty())
    {
        diagnostics.push_back(Diagnostic("Build.Snapshot.InvalidRequest",
            "A verified profile, RID, and private SagaRuntime are required."));
        return std::nullopt;
    }
    auto freshness = VerifyPlaySourceSnapshot(request.source);
    if (!freshness.empty()) { diagnostics = std::move(freshness); return std::nullopt; }

    BuildInputSnapshot snapshot;
    snapshot.projectId = request.source.projectId;
    snapshot.projectHash = request.source.projectHash;
    snapshot.lockHash = request.source.trackedLockHash;
    snapshot.profileId = request.profileId;
    snapshot.profileHash = request.profileHash;
    snapshot.target = request.target;
    snapshot.runtimeIdentifier = request.runtimeIdentifier;
    snapshot.configuration = request.configuration;
    std::error_code error;
    snapshot.runtimeHash = HashFile(request.runtimeExecutable, error);
    if (error)
    {
        diagnostics.push_back(Diagnostic("Build.Snapshot.RuntimeMissing",
            "The selected private SagaRuntime is missing.", request.runtimeExecutable));
        return std::nullopt;
    }
    ManagedToolRuntime managed(request.toolchain.managedRuntime);
    for (const auto id : {ManagedToolId::ProjectKit, ManagedToolId::Script,
                          ManagedToolId::Packager})
    {
        const auto descriptor = managed.Resolve(id);
        const auto hash = HashFile(descriptor.dll, error);
        if (error || hash.empty())
        {
            diagnostics.push_back(Diagnostic("Build.Snapshot.PrivateToolMissing",
                "A required private managed build tool is unavailable.", descriptor.dll));
            return std::nullopt;
        }
        snapshot.toolHashes[ManagedToolName(id)] = hash;
    }

    const auto packageRoot = request.source.projectRoot / "Build/Packages";
    const auto currentPath = packageRoot / "manifests/current.json";
    nlohmann::json current;
    if (!ReadJson(currentPath, current) || current.value("schemaVersion", -1) != 1)
    {
        diagnostics.push_back(Diagnostic("Build.Snapshot.GenerationInvalid",
            "A verified current package generation is required.", currentPath));
        return std::nullopt;
    }
    snapshot.packageGenerationId = current.value("generationId", std::string{});
    snapshot.packageGenerationHash = current.value("generationHash", std::string{});
    snapshot.packageGraphHash = current.value("graphHash", std::string{});
    if (!IsHash(snapshot.packageGenerationId) ||
        !IsHash(snapshot.packageGenerationHash) ||
        !IsHash(snapshot.packageGraphHash))
    {
        diagnostics.push_back(Diagnostic("Build.Snapshot.GenerationInvalid",
            "Package generation and graph identities must be SHA-256 values."));
        return std::nullopt;
    }

    std::set<std::filesystem::path> seen;
    const auto projectRelative = std::filesystem::relative(
        request.source.projectPath, request.source.projectRoot, error);
    if (error || !AddFile(snapshot, request.source.projectPath, projectRelative, seen, diagnostics) ||
        !AddFile(snapshot, request.source.trackedLockPath, "SagaPackage.lock", seen, diagnostics) ||
        !AddFile(snapshot, request.buildProfilesPath, "BuildProfiles.saga.json", seen, diagnostics))
        return std::nullopt;
    for (const auto& source : request.source.sources)
        if (!AddFile(snapshot, source.path, source.projectRelativePath, seen, diagnostics))
            return std::nullopt;
    if (!AddFile(snapshot, currentPath, "Build/Packages/manifests/current.json",
                 seen, diagnostics)) return std::nullopt;
    const auto generationRoot = packageRoot / "generations" / snapshot.packageGenerationId;
    if (!Contained(packageRoot, generationRoot))
    {
        diagnostics.push_back(Diagnostic("Build.Snapshot.GenerationEscape",
            "The selected package generation escaped the package root.", generationRoot));
        return std::nullopt;
    }
    std::vector<std::filesystem::path> generated;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(generationRoot, error))
    {
        if (error || entry.is_symlink()) break;
        if (entry.is_regular_file()) generated.push_back(entry.path());
    }
    if (error)
    {
        diagnostics.push_back(Diagnostic("Build.Snapshot.GenerationUnreadable",
            error.message(), generationRoot));
        return std::nullopt;
    }
    std::sort(generated.begin(), generated.end());
    for (const auto& path : generated)
        if (!AddFile(snapshot, path,
            std::filesystem::relative(path, request.source.projectRoot), seen, diagnostics))
            return std::nullopt;

    if (request.beforeIdentitySecondRead) request.beforeIdentitySecondRead();
    freshness = VerifyPlaySourceSnapshot(request.source);
    nlohmann::json second;
    if (!freshness.empty() || !ReadJson(currentPath, second) ||
        second.value("generationId", std::string{}) != snapshot.packageGenerationId ||
        second.value("generationHash", std::string{}) != snapshot.packageGenerationHash ||
        second.value("graphHash", std::string{}) != snapshot.packageGraphHash)
    {
        diagnostics.push_back(Diagnostic("Build.Snapshot.GenerationChanged",
            "Project or package identity changed while the snapshot was captured."));
        return std::nullopt;
    }
    std::sort(snapshot.files.begin(), snapshot.files.end(), [](const auto& left, const auto& right)
    { return left.snapshotPath.generic_string() < right.snapshotPath.generic_string(); });
    snapshot.canonicalJson = SnapshotJson(snapshot).dump() + "\n";
    snapshot.snapshotId = ContentId(snapshot.canonicalJson);
    auto finalJson = SnapshotJson(snapshot);
    finalJson["snapshotId"] = snapshot.snapshotId;
    snapshot.canonicalJson = finalJson.dump() + "\n";
    return snapshot;
}

StandaloneBuildResult StandaloneBuildCoordinator::Build(
    const StandaloneBuildRequest& request, StandaloneBuildExecutor executor) const
{
    StandaloneBuildResult result;
    result.outputDirectory = request.outputDirectory;
    if (!SafeOutput(request, result.diagnostics)) return result;
    Report(request, StandaloneBuildState::Capturing, 0, 0, "Capture snapshot");
    for (unsigned attempt = 0; attempt < std::max(1u, request.captureAttempts); ++attempt)
    {
        result.diagnostics.clear();
        auto captured = Capture(request, result.diagnostics);
        if (captured) { result.snapshot = std::move(*captured); break; }
        if (result.diagnostics.empty() ||
            result.diagnostics.back().code != "Build.Snapshot.GenerationChanged") return result;
    }
    if (result.snapshot.snapshotId.empty()) return result;
    if (request.cancellationRequested && request.cancellationRequested())
    { result.state = StandaloneBuildState::Cancelled; return result; }

    const auto staging = request.outputDirectory.parent_path() /
        (".saga-standalone-" + result.snapshot.snapshotId.substr(0, 16));
    const auto materialized = staging / "materialized";
    const auto candidate = staging / "candidate";
    std::error_code error;
    if (std::filesystem::exists(staging, error))
    {
        result.diagnostics.push_back(Diagnostic("Build.Staging.Collision",
            "Owned staging directory already exists; no path was removed.", staging));
        return result;
    }
    std::filesystem::create_directories(materialized, error);
    if (error)
    {
        result.diagnostics.push_back(Diagnostic("Build.Staging.CreateFailed",
            error.message(), staging));
        return result;
    }
    auto cleanup = [&]() { std::error_code ignored; std::filesystem::remove_all(staging, ignored); };
    unsigned copied = 0;
    for (const auto& file : result.snapshot.files)
    {
        const auto destination = materialized / file.snapshotPath;
        std::filesystem::create_directories(destination.parent_path(), error);
        if (!error) std::filesystem::copy_file(file.sourcePath, destination,
            std::filesystem::copy_options::overwrite_existing, error);
        if (error || HashFile(destination, error) != file.hash)
        {
            result.diagnostics.push_back(Diagnostic("Build.Materialize.VerificationFailed",
                "A materialized input failed byte verification.", destination));
            cleanup(); return result;
        }
        Report(request, StandaloneBuildState::Materializing, ++copied,
            static_cast<unsigned>(result.snapshot.files.size()),
            file.snapshotPath.generic_string());
    }
    if (!AtomicWriteDocument(materialized / "BuildInputSnapshot.json",
        result.snapshot.canonicalJson, result.snapshot.snapshotId, error))
    { cleanup(); return result; }

    PlaySourceSnapshot source = request.source;
    source.projectRoot = materialized;
    source.projectPath = materialized / std::filesystem::relative(
        request.source.projectPath, request.source.projectRoot);
    source.trackedLockPath = materialized / "SagaPackage.lock";
    source.sources.clear();
    for (const auto& original : request.source.sources)
        source.sources.push_back({original.kind, original.documentId,
            materialized / original.projectRelativePath,
            original.projectRelativePath, original.hash});
    source.workspaceGeneration = 1;
    source.workspaceFence = std::make_shared<const std::atomic_uint64_t>(1);

    Report(request, StandaloneBuildState::Building, 0, 0, "Build materialized snapshot");
    auto execution = (executor ? executor : DefaultExecute)(source, candidate, request);
    if (!execution.passed)
    {
        result.state = execution.cancelled ? StandaloneBuildState::Cancelled
                                           : StandaloneBuildState::Failed;
        result.diagnostics = std::move(execution.diagnostics);
        cleanup(); return result;
    }
    if (!CopyTree(materialized, candidate / "Project", error))
    {
        result.diagnostics.push_back(Diagnostic("Build.Stage.CopyFailed", error.message(), candidate));
        cleanup(); return result;
    }
    std::filesystem::create_directories(candidate / "bin", error);
    std::filesystem::copy_file(request.runtimeExecutable, candidate / "bin/SagaRuntime",
        std::filesystem::copy_options::overwrite_existing, error);
    if (error)
    {
        result.diagnostics.push_back(Diagnostic("Build.Stage.RuntimeCopyFailed", error.message()));
        cleanup(); return result;
    }

    if (!execution.localEnvelopePath.empty())
    {
        const auto envelopeRelative = std::filesystem::relative(
            execution.localEnvelopePath, materialized, error);
        const auto stagedEnvelope = candidate / "Project" / envelopeRelative;
        nlohmann::json envelope;
        if (error || !ReadJson(stagedEnvelope, envelope))
        {
            result.diagnostics.push_back(Diagnostic("Build.Stage.EnvelopeInvalid",
                "The prepared local launch envelope is missing or invalid.", stagedEnvelope));
            cleanup(); return result;
        }
        RebaseJsonPaths(envelope, materialized.string(),
            (request.outputDirectory / "Project").string(),
            request.runtimeExecutable.string(),
            (request.outputDirectory / "bin/SagaRuntime").string());
        if (!AtomicWriteDocument(stagedEnvelope, envelope.dump(2) + "\n",
            result.snapshot.snapshotId, error))
        {
            result.diagnostics.push_back(Diagnostic("Build.Stage.EnvelopeWriteFailed",
                error.message(), stagedEnvelope)); cleanup(); return result;
        }
    }

    result.artifactHashes = execution.hashes;
    result.artifactHashes["executable"] = HashFile(candidate / "bin/SagaRuntime", error);
    result.artifactHashes["snapshot"] = result.snapshot.snapshotId;
    if (error || result.artifactHashes["executable"] != result.snapshot.runtimeHash)
    {
        result.diagnostics.push_back(Diagnostic("Build.Verify.RuntimeHashMismatch",
            "Published runtime hash changed.")); cleanup(); return result;
    }
    Json identity = {{"artifacts", result.artifactHashes},
        {"configuration", result.snapshot.configuration},
        {"inputs", SnapshotJson(result.snapshot)},
        {"profileHash", result.snapshot.profileHash}, {"profileId", result.snapshot.profileId},
        {"runtimeIdentifier", result.snapshot.runtimeIdentifier}, {"schemaVersion", 1},
        {"snapshotId", result.snapshot.snapshotId}, {"target", result.snapshot.target}};
    const auto identityBytes = identity.dump() + "\n";
    result.manifestHash = ContentId(identityBytes);
    const auto generatedAt = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    Json manifest = {{"contentIdentity", identity},
        {"contentIdentityHash", result.manifestHash},
        {"generatedAtUnixSeconds", generatedAt}, {"schemaVersion", 1}};
    if (!AtomicWriteDocument(candidate / "StandaloneManifest.saga.json",
        manifest.dump(2) + "\n", result.snapshot.snapshotId, error))
    { cleanup(); return result; }
    Json run = {{"arguments", Json::array({"--play-envelope",
        (request.outputDirectory / "Project/Build/Editor/Play" /
         source.playSessionId / "Transport/local-launch-envelope-v1.json").string()})},
        {"executable", (request.outputDirectory / "bin/SagaRuntime").string()},
        {"schemaVersion", 1}};
    if (!AtomicWriteDocument(candidate / "StandaloneRun.saga.json",
        run.dump(2) + "\n", result.snapshot.snapshotId, error))
    { cleanup(); return result; }

    if (request.beforePublish) request.beforePublish();
    if (request.cancellationRequested && request.cancellationRequested())
    { result.state = StandaloneBuildState::Cancelled; cleanup(); return result; }
    Report(request, StandaloneBuildState::Publishing, 0, 0, "Publish verified output");
    const auto backup = staging / "previous";
    const bool hadPrevious = std::filesystem::exists(request.outputDirectory, error);
    Json journal = {{"candidate", candidate.string()},
        {"destination", request.outputDirectory.string()}, {"previous", backup.string()},
        {"schemaVersion", 1}, {"snapshotId", result.snapshot.snapshotId}};
    if (!AtomicWriteDocument(staging / "publish-journal.json", journal.dump(2) + "\n",
        result.snapshot.snapshotId, error)) { cleanup(); return result; }
    if (hadPrevious) std::filesystem::rename(request.outputDirectory, backup, error);
    if (!error && request.publicationAllowed && !request.publicationAllowed())
        error = std::make_error_code(std::errc::operation_canceled);
    if (!error) std::filesystem::rename(candidate, request.outputDirectory, error);
    if (error)
    {
        if (hadPrevious && !std::filesystem::exists(request.outputDirectory))
        { std::error_code restore; std::filesystem::rename(backup, request.outputDirectory, restore); }
        result.preservedPreviousOutput = hadPrevious;
        result.diagnostics.push_back(Diagnostic("Build.Publish.Failed",
            "Publication failed and the prior valid output was preserved.",
            request.outputDirectory));
        cleanup(); return result;
    }
    result.preservedPreviousOutput = hadPrevious;
    result.published = true;
    result.state = StandaloneBuildState::Succeeded;
    result.manifestPath = request.outputDirectory / "StandaloneManifest.saga.json";
    cleanup();
    Report(request, StandaloneBuildState::Succeeded, 1, 1,
        "Standalone build published");
    return result;
}

std::string StandaloneBuildCoordinator::ClassifyReproducibility(
    const StandaloneBuildResult& first, const StandaloneBuildResult& second)
{
    if (!first.Succeeded() || !second.Succeeded() ||
        first.snapshot.snapshotId != second.snapshot.snapshotId) return "not-comparable";
    std::map<std::filesystem::path, std::string> firstFiles, secondFiles;
    auto inventory = [](const std::filesystem::path& root,
                        std::map<std::filesystem::path, std::string>& output)
    {
        std::error_code error;
        for (const auto& entry : std::filesystem::recursive_directory_iterator(root, error))
        {
            if (error || entry.is_symlink()) return false;
            if (!entry.is_regular_file()) continue;
            const auto relative = std::filesystem::relative(entry.path(), root, error);
            const auto hash = error ? std::string{} : HashFile(entry.path(), error);
            if (error) return false;
            output[relative] = hash;
        }
        return true;
    };
    if (!inventory(first.outputDirectory, firstFiles) ||
        !inventory(second.outputDirectory, secondFiles)) return "not-comparable";
    return firstFiles == secondFiles ? "byte-reproducible"
                                     : "semantic-content-addressed";
}
} // namespace SagaEditor
