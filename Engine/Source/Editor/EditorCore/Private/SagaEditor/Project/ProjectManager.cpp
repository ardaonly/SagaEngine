// SPDX-License-Identifier: MPL-2.0

/// @file ProjectManager.cpp
/// @brief Editor project session backed by SagaProjectKit validation.

#include "SagaEditor/Project/ProjectManager.h"
#include "SagaEditor/Project/SagaProjectKitClient.h"

#include <algorithm>
#include <cstdlib>
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
    const char* configured = std::getenv("SAGA_PROJECT_TOOL");
    if (configured && *configured)
        return std::make_shared<SagaProjectKitClient>(configured);
    const auto findFrom = [](std::filesystem::path cursor) -> std::filesystem::path
    {
        std::error_code error;
        cursor = std::filesystem::absolute(cursor, error);
        if (error) return {};
        while (!cursor.empty())
        {
            const auto candidate = cursor /
                "Tools/SagaProjectKit/bin/Release/net10.0/sagaproject";
            if (std::filesystem::is_regular_file(candidate)) return candidate;
            const auto parent = cursor.parent_path();
            if (parent == cursor) break;
            cursor = parent;
        }
        return {};
    };
    if (const char* engineRoot = std::getenv("SAGA_ENGINE_ROOT"); engineRoot && *engineRoot)
        if (const auto found = findFrom(engineRoot); !found.empty())
            return std::make_shared<SagaProjectKitClient>(found);
    if (const auto found = findFrom(std::filesystem::current_path()); !found.empty())
        return std::make_shared<SagaProjectKitClient>(found);
#ifndef _WIN32
    std::error_code error;
    const auto executable = std::filesystem::read_symlink("/proc/self/exe", error);
    if (!error)
        if (const auto found = findFrom(executable.parent_path()); !found.empty())
            return std::make_shared<SagaProjectKitClient>(found);
#endif
    return std::make_shared<SagaProjectKitClient>("sagaproject");
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

bool ProjectManager::CreateStarterArena(const ProjectTemplateRequest& request)
{
    const auto creation = m_impl->client->CreateStarterArena(request);
    if (!creation.passed) return false;
    return OpenProject(request.destination.string());
}

bool ProjectManager::OpenProject(const std::string& path)
{
    const auto manifest = ResolveManifestPath(path);
    if (manifest.empty()) return false;
    const auto validation = m_impl->client->Validate(manifest);
    if (!validation.passed) return false;
    ProjectInfo next;
    next.projectId = validation.project.projectId;
    next.name = validation.project.displayName;
    next.rootPath = validation.project.projectRoot.string();
    next.manifestPath = validation.project.manifestPath.string();
    next.engineVersion = validation.project.engineTargetVersion;
    next.schemaVersion = validation.project.schemaVersion;
    next.sources = validation.project.sources;
    m_impl->current = std::move(next);
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
