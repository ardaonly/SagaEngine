// SPDX-License-Identifier: MPL-2.0

#pragma once

#include "SagaEditor/Documents/SourceDocuments.h"
#include "SagaEditor/Project/ProjectManager.h"
#include "SagaEditor/SceneEditing/SceneDocument.h"

#include <memory>
#include <atomic>
#include <functional>
#include <map>
#include <vector>

namespace SagaEditor
{

enum class ProjectSourceClassification : std::uint8_t
{
    SourceTruth,
    GeneratedStateTrackedException,
    GeneratedState,
    PersonalState,
    SharedWorkspaceState,
};

struct ProjectSourceEntry
{
    std::string id;
    std::string kind;
    std::filesystem::path relativePath;
    ProjectSourceClassification classification = ProjectSourceClassification::SourceTruth;
};

class EditorDocumentWorkspace
{
public:
    using ChangeSubscription = std::uint64_t;
    using ChangeCallback = std::function<void()>;
    static constexpr ChangeSubscription InvalidChangeSubscription = 0;
    explicit EditorDocumentWorkspace(std::shared_ptr<IProjectToolClient> client = {});
    ~EditorDocumentWorkspace();

    [[nodiscard]] bool OpenProject(const std::filesystem::path& path);
    [[nodiscard]] ProjectToolResult CreateStarterArena(
        const ProjectTemplateRequest& request);
    [[nodiscard]] ProjectToolResult CreateProject(
        const ProjectTemplateRequest& request);
    void Close();

    [[nodiscard]] bool IsOpen() const noexcept;
    [[nodiscard]] const ProjectInfo* Project() const noexcept;
    [[nodiscard]] ProjectDocument* ProjectSource() const noexcept;
    [[nodiscard]] SceneDocument* ActiveScene() const noexcept;
    [[nodiscard]] const std::vector<std::unique_ptr<SceneDocument>>& Scenes() const noexcept;
    [[nodiscard]] const std::vector<std::unique_ptr<CSharpSourceDocument>>& Scripts() const noexcept;
    [[nodiscard]] const std::vector<std::unique_ptr<InputMapDocument>>& InputMaps() const noexcept;
    [[nodiscard]] const std::vector<ProjectSourceEntry>& SourceListing() const noexcept;
    [[nodiscard]] std::vector<std::string> DirtyDocumentIds() const;
    [[nodiscard]] bool SaveAll();
    [[nodiscard]] std::uint64_t Generation() const noexcept;
    [[nodiscard]] std::shared_ptr<const std::atomic_uint64_t> RevisionFence() const noexcept;
    [[nodiscard]] ChangeSubscription SubscribeChanges(ChangeCallback callback);
    void UnsubscribeChanges(ChangeSubscription subscription);

private:
    [[nodiscard]] bool OpenValidated(std::unique_ptr<ProjectManager> manager);
    void MarkChanged();

    std::shared_ptr<IProjectToolClient> client_;
    std::unique_ptr<ProjectManager> manager_;
    std::unique_ptr<ProjectDocument> projectSource_;
    std::vector<std::unique_ptr<SceneDocument>> scenes_;
    std::vector<std::unique_ptr<CSharpSourceDocument>> scripts_;
    std::vector<std::unique_ptr<InputMapDocument>> inputs_;
    std::vector<ProjectSourceEntry> listing_;
    std::map<ChangeSubscription, ChangeCallback> callbacks_;
    ChangeSubscription nextSubscription_ = 1;
    std::shared_ptr<std::atomic_uint64_t> revisionFence_ =
        std::make_shared<std::atomic_uint64_t>(1);
};

} // namespace SagaEditor
