// SPDX-FileCopyrightText: 2026 Arda Koyuncu
// SPDX-License-Identifier: MPL-2.0

#include "SagaEditor/Workspace/EditorDocumentWorkspace.h"

#include <algorithm>

namespace SagaEditor
{
EditorDocumentWorkspace::EditorDocumentWorkspace(std::shared_ptr<IProjectToolClient> client)
    : client_(std::move(client)) {}
EditorDocumentWorkspace::~EditorDocumentWorkspace()
{
    revisionFence_->fetch_add(1, std::memory_order_release);
}

bool EditorDocumentWorkspace::OpenProject(const std::filesystem::path& path)
{
    auto next = std::make_unique<ProjectManager>(client_);
    if (!next->OpenProject(path.string())) return false;
    return OpenValidated(std::move(next));
}

bool EditorDocumentWorkspace::CreateStarterArena(const ProjectTemplateRequest& request)
{
    auto next = std::make_unique<ProjectManager>(client_);
    if (!next->CreateStarterArena(request)) return false;
    return OpenValidated(std::move(next));
}

bool EditorDocumentWorkspace::OpenValidated(std::unique_ptr<ProjectManager> nextManager)
{
    const auto& project = nextManager->GetCurrentProject();
    auto nextProject = std::make_unique<ProjectDocument>(project.manifestPath);
    if (!nextProject->Load()) return false;
    std::vector<std::unique_ptr<SceneDocument>> nextScenes;
    std::vector<std::unique_ptr<CSharpSourceDocument>> nextScripts;
    std::vector<std::unique_ptr<InputMapDocument>> nextInputs;
    std::vector<ProjectSourceEntry> nextListing{{"project", "project",
        std::filesystem::path(project.manifestPath).filename(),
        ProjectSourceClassification::SourceTruth}};

    for (const auto& source : project.sources)
    {
        nextListing.push_back({source.id, source.kind, source.relativePath,
            ProjectSourceClassification::SourceTruth});
        if (source.category == "scene")
        {
            auto document = std::make_unique<SceneDocument>(source.absolutePath);
            if (!document->Load()) return false;
            nextScenes.push_back(std::move(document));
        }
        else if (source.category == "inputMap")
        {
            auto document = std::make_unique<InputMapDocument>(source.absolutePath);
            if (!document->Load()) return false;
            nextInputs.push_back(std::move(document));
        }
        else if (source.category == "scriptFolder")
        {
            std::error_code error;
            std::vector<std::filesystem::path> paths;
            for (const auto& entry : std::filesystem::recursive_directory_iterator(source.absolutePath, error))
                if (entry.is_regular_file() && entry.path().extension() == ".cs") paths.push_back(entry.path());
            if (error) return false;
            std::sort(paths.begin(), paths.end());
            for (const auto& path : paths)
            {
                auto document = std::make_unique<CSharpSourceDocument>(path);
                if (!document->Load()) return false;
                nextListing.push_back({"script:" + path.filename().string(), "csharp-source",
                    std::filesystem::relative(path, project.rootPath), ProjectSourceClassification::SourceTruth});
                nextScripts.push_back(std::move(document));
            }
        }
    }
    const auto lock = std::filesystem::path(project.rootPath) / "SagaPackage.lock";
    if (std::filesystem::is_regular_file(lock))
        nextListing.push_back({"package-lock", "package-lock", "SagaPackage.lock",
            ProjectSourceClassification::GeneratedStateTrackedException});
    std::sort(nextListing.begin(), nextListing.end(), [](const auto& left, const auto& right)
    {
        return left.relativePath.generic_string() < right.relativePath.generic_string();
    });
    manager_ = std::move(nextManager);
    projectSource_ = std::move(nextProject);
    scenes_ = std::move(nextScenes);
    scripts_ = std::move(nextScripts);
    inputs_ = std::move(nextInputs);
    listing_ = std::move(nextListing);
    for (const auto& scene : scenes_)
        (void)scene->SubscribeChanges([this] { MarkChanged(); });
    for (const auto& script : scripts_)
        (void)script->SubscribeChanges([this] { MarkChanged(); });
    for (const auto& input : inputs_)
        (void)input->SubscribeChanges([this] { MarkChanged(); });
    MarkChanged();
    return true;
}

void EditorDocumentWorkspace::Close()
{
    if (manager_) manager_->CloseProject();
    manager_.reset();
    projectSource_.reset();
    scenes_.clear(); scripts_.clear(); inputs_.clear(); listing_.clear();
    MarkChanged();
}

void EditorDocumentWorkspace::MarkChanged()
{
    revisionFence_->fetch_add(1, std::memory_order_release);
    const auto callbacks = callbacks_;
    for (const auto& [subscription, callback] : callbacks)
    { (void)subscription; if (callback) callback(); }
}

bool EditorDocumentWorkspace::IsOpen() const noexcept { return manager_ && manager_->IsProjectOpen(); }
const ProjectInfo* EditorDocumentWorkspace::Project() const noexcept
{ return IsOpen() ? &manager_->GetCurrentProject() : nullptr; }
ProjectDocument* EditorDocumentWorkspace::ProjectSource() const noexcept { return projectSource_.get(); }
SceneDocument* EditorDocumentWorkspace::ActiveScene() const noexcept
{ return scenes_.empty() ? nullptr : scenes_.front().get(); }
const std::vector<std::unique_ptr<SceneDocument>>& EditorDocumentWorkspace::Scenes() const noexcept { return scenes_; }
const std::vector<std::unique_ptr<CSharpSourceDocument>>& EditorDocumentWorkspace::Scripts() const noexcept { return scripts_; }
const std::vector<std::unique_ptr<InputMapDocument>>& EditorDocumentWorkspace::InputMaps() const noexcept { return inputs_; }
const std::vector<ProjectSourceEntry>& EditorDocumentWorkspace::SourceListing() const noexcept { return listing_; }

std::vector<std::string> EditorDocumentWorkspace::DirtyDocumentIds() const
{
    std::vector<std::string> ids;
    const auto append = [&ids](const auto& documents)
    {
        for (const auto& document : documents)
            if (document->Snapshot().dirty) ids.push_back(document->Snapshot().documentId);
    };
    append(scenes_); append(scripts_); append(inputs_);
    std::sort(ids.begin(), ids.end());
    return ids;
}

bool EditorDocumentWorkspace::SaveAll()
{
    const auto save = [](const auto& documents)
    {
        for (const auto& document : documents)
        {
            if (!document->Snapshot().dirty) continue;
            if (document->Save() != DocumentSaveStatus::Saved) return false;
        }
        return true;
    };
    return save(scenes_) && save(scripts_) && save(inputs_);
}

std::uint64_t EditorDocumentWorkspace::Generation() const noexcept
{ return revisionFence_->load(std::memory_order_acquire); }

std::shared_ptr<const std::atomic_uint64_t>
EditorDocumentWorkspace::RevisionFence() const noexcept
{ return revisionFence_; }

EditorDocumentWorkspace::ChangeSubscription EditorDocumentWorkspace::SubscribeChanges(
    ChangeCallback callback)
{
    if (!callback) return InvalidChangeSubscription;
    const auto subscription = nextSubscription_++;
    callbacks_.emplace(subscription, std::move(callback));
    return subscription;
}

void EditorDocumentWorkspace::UnsubscribeChanges(ChangeSubscription subscription)
{
    callbacks_.erase(subscription);
}

} // namespace SagaEditor
