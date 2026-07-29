// SPDX-License-Identifier: MPL-2.0

/// @file ProjectManager.cpp
/// @brief Editor project session backed by SagaProjectKit validation.

#include "SagaEditor/Project/ProjectManager.h"
#include "SagaEditor/Project/SagaProjectKitClient.h"

#include <algorithm>
#include <filesystem>
#include <utility>
#include <vector>

namespace SagaEditor
{
namespace
{
std::filesystem::path ResolveManifestPath(const std::filesystem::path& input)
{
    std::error_code error;
    if (std::filesystem::is_regular_file(input, error))
        return input.extension() == ".sagaproj" ?
            std::filesystem::absolute(input).lexically_normal() : std::filesystem::path{};
    if (!std::filesystem::is_directory(input, error)) return {};
    std::vector<std::filesystem::path> candidates;
    for (const auto& entry : std::filesystem::directory_iterator(input, error))
        if (entry.is_regular_file() && entry.path().extension() == ".sagaproj")
            candidates.push_back(entry.path());
    if (error || candidates.size() != 1) return {};
    return std::filesystem::absolute(candidates.front()).lexically_normal();
}

std::shared_ptr<IProjectToolClient> DefaultClient()
{
    return std::make_shared<SagaProjectKitClient>(
        ResolveEditorToolchain().managedRuntime);
}

ProjectInfo ProjectInfoFrom(const ValidatedProjectDescriptor& project)
{
    ProjectInfo next;
    next.projectId = project.projectId;
    next.name = project.displayName;
    next.rootPath = project.projectRoot.string();
    next.manifestPath = project.manifestPath.string();
    next.engineVersion = project.engineTargetVersion;
    next.schemaVersion = project.schemaVersion;
    next.sources = project.sources;
    return next;
}
}

struct ProjectManager::Impl
{
    std::shared_ptr<IProjectToolClient> client;
    bool open = false;
    ProjectInfo current;
    std::function<void()> onProjectChanged;
};

ProjectManager::ProjectManager(std::shared_ptr<IProjectToolClient> client)
    : m_impl(std::make_unique<Impl>())
{
    m_impl->client = client ? std::move(client) : DefaultClient();
}

ProjectManager::~ProjectManager() = default;

ProjectToolResult ProjectManager::CreateStarterArena(
    const ProjectTemplateRequest& request)
{
    return CreateProject(request);
}

ProjectToolResult ProjectManager::CreateProject(
    const ProjectTemplateRequest& request)
{
    auto creation = m_impl->client->CreateProject(request);
    if (!creation.passed) return creation;
    m_impl->current = ProjectInfoFrom(creation.project);
    m_impl->open = true;
    if (m_impl->onProjectChanged) m_impl->onProjectChanged();
    return creation;
}

bool ProjectManager::OpenProject(const std::string& path)
{
    const auto manifest = ResolveManifestPath(path);
    if (manifest.empty()) return false;
    const auto validation = m_impl->client->Validate(manifest);
    if (!validation.passed) return false;
    m_impl->current = ProjectInfoFrom(validation.project);
    m_impl->open = true;
    if (m_impl->onProjectChanged) m_impl->onProjectChanged();
    return true;
}

void ProjectManager::CloseProject()
{
    if (!m_impl->open) return;
    m_impl->current = {};
    m_impl->open = false;
    if (m_impl->onProjectChanged) m_impl->onProjectChanged();
}

bool ProjectManager::IsProjectOpen() const noexcept { return m_impl->open; }
const ProjectInfo& ProjectManager::GetCurrentProject() const { return m_impl->current; }
void ProjectManager::SetOnProjectChanged(std::function<void()> cb) { m_impl->onProjectChanged = std::move(cb); }

} // namespace SagaEditor
