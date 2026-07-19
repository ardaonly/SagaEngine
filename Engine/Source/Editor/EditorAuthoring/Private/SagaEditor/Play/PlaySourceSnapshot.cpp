// SPDX-License-Identifier: MPL-2.0

#include "SagaEditor/Play/PlaySourceSnapshot.h"

#include "SagaEditor/Workspace/EditorDocumentWorkspace.h"

#include <algorithm>
#include <set>

namespace SagaEditor
{
namespace
{
DocumentDiagnostic Diagnostic(std::string code, std::string message,
                              std::filesystem::path path = {})
{ return {std::move(code), std::move(message), std::move(path), 0, 0}; }

std::string HashFile(const std::filesystem::path& path, std::error_code& error)
{
    const auto bytes = ReadDocumentBytes(path, error);
    return error ? std::string{} : HashDocumentBytes(bytes);
}
} // namespace

PlaySourceSnapshotResult CapturePlaySourceSnapshot(
    const EditorDocumentWorkspace& workspace, std::string playSessionId)
{
    PlaySourceSnapshotResult result;
    if (!workspace.IsOpen() || !workspace.Project() || !workspace.ProjectSource() ||
        playSessionId.empty())
    {
        result.diagnostics.push_back(Diagnostic("Play.Snapshot.ProjectNotOpen",
            "An open canonical project and play-session identity are required."));
        return result;
    }
    if (!workspace.DirtyDocumentIds().empty())
    {
        result.diagnostics.push_back(Diagnostic("Play.Prepare.UnsavedDocuments",
            "Play consumes saved disk source. Save All or cancel Play."));
        return result;
    }

    PlaySourceSnapshot snapshot;
    snapshot.playSessionId = std::move(playSessionId);
    snapshot.projectId = workspace.Project()->projectId;
    snapshot.projectRoot = workspace.Project()->rootPath;
    snapshot.projectPath = workspace.Project()->manifestPath;
    snapshot.trackedLockPath = snapshot.projectRoot / "SagaPackage.lock";
    snapshot.workspaceGeneration = workspace.Generation();
    snapshot.workspaceFence = workspace.RevisionFence();
    std::error_code error;
    snapshot.projectHash = HashFile(snapshot.projectPath, error);
    if (error)
    {
        result.diagnostics.push_back(Diagnostic("Play.Snapshot.RequiredFileMissing",
            "Canonical project manifest must be readable.", snapshot.projectPath));
        return result;
    }
    snapshot.trackedLockHash = HashFile(snapshot.trackedLockPath, error);
    if (error)
    {
        result.diagnostics.push_back(Diagnostic("Play.Snapshot.RequiredFileMissing",
            "Tracked SagaPackage.lock must be readable.", snapshot.trackedLockPath));
        return result;
    }

    auto addDocument = [&](std::string kind, const IDocument& document)
    {
        const auto& state = document.Snapshot();
        auto relative = std::filesystem::relative(
            state.canonicalSourcePath, snapshot.projectRoot, error);
        if (error || relative.empty() || relative.is_absolute() ||
            *relative.begin() == std::filesystem::path("..")) return;
        const auto hash = HashFile(state.canonicalSourcePath, error);
        if (error) return;
        snapshot.sources.push_back({std::move(kind), state.documentId,
            state.canonicalSourcePath, std::move(relative), hash});
        snapshot.requiredDocumentIds.push_back(state.documentId);
    };
    for (const auto& scene : workspace.Scenes()) addDocument("scene", *scene);
    for (const auto& script : workspace.Scripts()) addDocument("script", *script);
    for (const auto& input : workspace.InputMaps()) addDocument("inputMap", *input);
    std::set<std::filesystem::path> capturedPaths;
    for (const auto& source : snapshot.sources)
        capturedPaths.insert(source.path.lexically_normal());
    auto addSourceFile = [&](const ValidatedProjectDescriptor::SourceReference& owner,
                             const std::filesystem::path& path)
    {
        if (!capturedPaths.insert(path.lexically_normal()).second) return;
        auto relative = std::filesystem::relative(path, snapshot.projectRoot, error);
        if (error || relative.empty() || relative.is_absolute() ||
            *relative.begin() == std::filesystem::path("..")) return;
        const auto hash = HashFile(path, error);
        if (error) return;
        snapshot.sources.push_back({owner.category, owner.id, path, std::move(relative), hash});
    };
    for (const auto& source : workspace.Project()->sources)
    {
        if (std::filesystem::is_regular_file(source.absolutePath, error))
            addSourceFile(source, source.absolutePath);
        else if (!error && std::filesystem::is_directory(source.absolutePath, error))
        {
            std::vector<std::filesystem::path> paths;
            for (const auto& entry : std::filesystem::recursive_directory_iterator(
                     source.absolutePath, error))
                if (entry.is_regular_file()) paths.push_back(entry.path());
            std::sort(paths.begin(), paths.end());
            for (const auto& path : paths) addSourceFile(source, path);
        }
        if (error) break;
    }
    if (error)
    {
        result.diagnostics.push_back(Diagnostic("Play.Snapshot.SourceReadFailed",
            error.message()));
        return result;
    }
    std::sort(snapshot.sources.begin(), snapshot.sources.end(), [](const auto& left, const auto& right)
    { return left.projectRelativePath.generic_string() < right.projectRelativePath.generic_string(); });
    std::sort(snapshot.requiredDocumentIds.begin(), snapshot.requiredDocumentIds.end());
    if (snapshot.sources.empty())
    {
        result.diagnostics.push_back(Diagnostic("Play.Snapshot.SourceSetEmpty",
            "The validated project source set is empty."));
        return result;
    }
    if (snapshot.workspaceFence->load(std::memory_order_acquire) != snapshot.workspaceGeneration)
    {
        result.diagnostics.push_back(Diagnostic("Play.Prepare.StaleSource",
            "Workspace changed while the saved-source snapshot was captured."));
        return result;
    }
    result.snapshot = std::move(snapshot);
    return result;
}

std::vector<DocumentDiagnostic> VerifyPlaySourceSnapshot(const PlaySourceSnapshot& snapshot)
{
    if (!snapshot.workspaceFence ||
        snapshot.workspaceFence->load(std::memory_order_acquire) != snapshot.workspaceGeneration)
        return {Diagnostic("Play.Prepare.StaleSource",
            "Workspace was closed, replaced, saved, or edited during Play preparation.")};
    std::error_code error;
    if (HashFile(snapshot.projectPath, error) != snapshot.projectHash || error)
        return {Diagnostic("Play.Prepare.StaleSource",
            "Canonical project bytes changed during Play preparation.", snapshot.projectPath)};
    if (HashFile(snapshot.trackedLockPath, error) != snapshot.trackedLockHash || error)
        return {Diagnostic("Play.Prepare.StaleSource",
            "Tracked SagaPackage.lock changed during Play preparation.", snapshot.trackedLockPath)};
    for (const auto& source : snapshot.sources)
        if (HashFile(source.path, error) != source.hash || error)
            return {Diagnostic("Play.Prepare.StaleSource",
                "Saved source changed during Play preparation.", source.path)};
    return {};
}
} // namespace SagaEditor
