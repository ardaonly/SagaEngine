// SPDX-License-Identifier: MPL-2.0

#include "SagaEditor/Viewport/ViewportContentBuildCoordinator.h"

#include "SagaEditor/Documents/Document.h"
#include "SagaEditor/Workspace/EditorDocumentWorkspace.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <fstream>
#include <iterator>
#include <vector>

namespace SagaEditor
{
namespace
{
std::string HashFile(const std::filesystem::path& path)
{
    std::error_code error;
    const auto bytes = ReadDocumentBytes(path, error);
    return error ? std::string{} : HashDocumentBytes(bytes);
}

std::string HashTree(const std::filesystem::path& path)
{
    std::error_code error;
    std::vector<std::filesystem::path> files;
    if (std::filesystem::is_regular_file(path, error)) files.push_back(path);
    else if (std::filesystem::is_directory(path, error))
    {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(path, error))
            if (entry.is_regular_file()) files.push_back(entry.path());
    }
    if (error) return {};
    std::sort(files.begin(), files.end());
    std::string material;
    for (const auto& file : files)
    {
        material += std::filesystem::relative(file, path.parent_path(), error).generic_string();
        material += ':';
        material += HashFile(file);
        material += '\n';
        if (error) return {};
    }
    return HashDocumentBytes(material);
}

bool SameContentIdentity(const ViewportContentBuildRequest& left,
                         const ViewportContentBuildRequest& right)
{
    auto normalizedLeft = left;
    auto normalizedRight = right;
    normalizedLeft.operationId.clear();
    normalizedRight.operationId.clear();
    return normalizedLeft == normalizedRight;
}
}

std::optional<ViewportContentBuildRequest> ViewportContentBuildCoordinator::Request(
    ViewportContentBuildRequest request)
{
    cancelled_ = false;
    if (active_)
    {
        pending_ = std::move(request);
        return std::nullopt;
    }
    active_ = std::move(request);
    return active_;
}

ViewportContentBuildCompletion ViewportContentBuildCoordinator::Complete(
    ViewportContentBuildResult result,
    const std::optional<ViewportContentBuildRequest>& currentWorkspaceIdentity)
{
    ViewportContentBuildCompletion completion;
    if (!active_ || result.request != *active_)
    {
        completion.stale = true;
        completion.diagnosticCode = "EditorRender.ContentBuild.UnknownCompletion";
        completion.diagnosticMessage = "A viewport build completed outside the active operation.";
        return completion;
    }

    const bool current = currentWorkspaceIdentity &&
        SameContentIdentity(result.request, *currentWorkspaceIdentity);
    const bool newest = !pending_;
    if (cancelled_ || result.cancelled)
    {
        completion.diagnosticCode = "EditorRender.ContentBuild.Cancelled";
        completion.diagnosticMessage = "Viewport content build was cancelled.";
    }
    else if (!current || !newest)
    {
        completion.stale = true;
        completion.diagnosticCode = "EditorRender.ContentBuild.StaleResult";
        completion.diagnosticMessage =
            "Viewport content changed while this immutable build request was active.";
    }
    else if (!result.passed)
    {
        completion.diagnosticCode = result.diagnosticCode.empty()
            ? "EditorRender.ContentBuild.Failed" : result.diagnosticCode;
        completion.diagnosticMessage = std::move(result.diagnosticMessage);
    }
    else
    {
        publishedRoot_ = result.contentRoot;
        completion.publish = true;
        completion.contentRoot = publishedRoot_;
    }

    active_.reset();
    cancelled_ = false;
    if (pending_)
    {
        active_ = std::move(pending_);
        pending_.reset();
        completion.nextRequest = active_;
    }
    return completion;
}

void ViewportContentBuildCoordinator::Cancel() noexcept
{
    cancelled_ = true;
    pending_.reset();
}

std::optional<ViewportContentBuildRequest> CaptureViewportContentBuildRequest(
    const EditorDocumentWorkspace& workspace, std::string operationId)
{
    const auto* project = workspace.Project();
    const auto* projectSource = workspace.ProjectSource();
    const auto* scene = workspace.ActiveScene();
    if (!project || !projectSource || !scene) return std::nullopt;

    ViewportContentBuildRequest request;
    request.operationId = std::move(operationId);
    request.workspaceGeneration = workspace.Generation();
    request.requestedAtRevision = workspace.RevisionFence()->load(std::memory_order_acquire);
    request.projectIdentity = project->projectId;
    request.projectRoot = project->rootPath;
    request.projectManifestPath = project->manifestPath;
    request.projectManifestHash = HashFile(request.projectManifestPath);
    request.sceneSourceIdentity = scene->SceneId() + ":" +
        scene->Snapshot().canonicalSourcePath.generic_string();
    request.sceneSourceHash = HashDocumentBytes(scene->Snapshot().baseDiskHash + ":" +
        std::to_string(scene->Snapshot().inMemoryRevision));

    std::string assetMaterial;
    for (const auto& source : project->sources)
    {
        if (source.category == "scene" || source.category == "scriptFolder" ||
            source.category == "inputMap") continue;
        assetMaterial += source.id + ":" + source.relativePath.generic_string() + ":" +
            HashTree(source.absolutePath) + "\n";
    }
    request.assetSourceHash = HashDocumentBytes(assetMaterial);

    const auto packageCurrent = request.projectRoot / "Build/Packages/current.json";
    try
    {
        std::ifstream input(packageCurrent, std::ios::binary);
        const auto json = nlohmann::json::parse(input);
        request.packageGenerationIdentity = json.value("generationId", std::string{});
        request.packageGenerationHash = json.value("generationHash", std::string{});
    }
    catch (const nlohmann::json::exception&)
    {
        request.packageGenerationIdentity.clear();
        request.packageGenerationHash.clear();
    }
    if (request.projectManifestHash.empty() || request.sceneSourceHash.empty() ||
        request.assetSourceHash.empty()) return std::nullopt;
    return request;
}

} // namespace SagaEditor
