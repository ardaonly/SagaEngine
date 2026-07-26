// SPDX-License-Identifier: MPL-2.0

#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>

namespace SagaEditor
{

class EditorDocumentWorkspace;

struct ViewportContentBuildRequest final
{
    std::string operationId;
    std::uint64_t workspaceGeneration = 0;
    std::string projectIdentity;
    std::filesystem::path projectRoot;
    std::filesystem::path projectManifestPath;
    std::string projectManifestHash;
    std::string sceneSourceIdentity;
    std::string sceneSourceHash;
    std::string assetSourceHash;
    std::string packageGenerationIdentity;
    std::string packageGenerationHash;
    std::uint64_t requestedAtRevision = 0;

    bool operator==(const ViewportContentBuildRequest&) const = default;
};

struct ViewportContentBuildResult final
{
    ViewportContentBuildRequest request;
    bool passed = false;
    bool cancelled = false;
    std::filesystem::path contentRoot;
    std::string diagnosticCode;
    std::string diagnosticMessage;
};

struct ViewportContentBuildCompletion final
{
    bool publish = false;
    bool stale = false;
    std::filesystem::path contentRoot;
    std::string diagnosticCode;
    std::string diagnosticMessage;
    std::optional<ViewportContentBuildRequest> nextRequest;
};

/// Qt-free single-flight publication barrier for immutable viewport content.
class ViewportContentBuildCoordinator final
{
public:
    [[nodiscard]] std::optional<ViewportContentBuildRequest> Request(
        ViewportContentBuildRequest request);
    [[nodiscard]] ViewportContentBuildCompletion Complete(
        ViewportContentBuildResult result,
        const std::optional<ViewportContentBuildRequest>& currentWorkspaceIdentity);
    void Cancel() noexcept;

    [[nodiscard]] bool Active() const noexcept { return active_.has_value(); }
    [[nodiscard]] bool Pending() const noexcept { return pending_.has_value(); }
    [[nodiscard]] const std::filesystem::path& PublishedRoot() const noexcept
    { return publishedRoot_; }

private:
    std::optional<ViewportContentBuildRequest> active_;
    std::optional<ViewportContentBuildRequest> pending_;
    std::filesystem::path publishedRoot_;
    bool cancelled_ = false;
};

[[nodiscard]] std::optional<ViewportContentBuildRequest>
CaptureViewportContentBuildRequest(const EditorDocumentWorkspace& workspace,
                                   std::string operationId);

} // namespace SagaEditor
