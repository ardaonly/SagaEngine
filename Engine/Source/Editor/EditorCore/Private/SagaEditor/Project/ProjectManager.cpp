/// @file ProjectManager.cpp
/// @brief Editor-local project session state.

#include "SagaEditor/Project/ProjectManager.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <utility>
#include <vector>

namespace SagaEditor
{
namespace
{

[[nodiscard]] std::filesystem::path ResolveProjectManifest(
    const std::filesystem::path& input)
{
    if (input.empty())
    {
        return {};
    }

    std::error_code ec;
    if (std::filesystem::is_regular_file(input, ec))
    {
        if (input.extension() != ".sagaproj")
            return {};
        return std::filesystem::weakly_canonical(input, ec);
    }

    if (!std::filesystem::is_directory(input, ec))
        return {};

    std::vector<std::filesystem::path> candidates;
    for (const auto& entry : std::filesystem::directory_iterator(input, ec))
    {
        if (entry.is_regular_file() && entry.path().extension() == ".sagaproj")
            candidates.push_back(entry.path());
    }
    if (ec || candidates.size() != 1)
        return {};
    return std::filesystem::weakly_canonical(candidates.front(), ec);
}

[[nodiscard]] bool ReadManifest(
    const std::filesystem::path& manifestPath,
    ProjectInfo& project)
{
    std::ifstream input(manifestPath);
    if (!input.is_open())
    {
        return false;
    }

    nlohmann::json json;
    try
    {
        input >> json;
    }
    catch (const nlohmann::json::exception&)
    {
        return false;
    }

    if (!json.is_object() || !json.contains("schemaVersion") ||
        !json["schemaVersion"].is_number_integer() ||
        json["schemaVersion"].get<int>() != 0 ||
        !json.contains("projectId") || !json["projectId"].is_string() ||
        json["projectId"].get<std::string>().empty() ||
        !json.contains("displayName") || !json["displayName"].is_string() ||
        json["displayName"].get<std::string>().empty())
        return false;

    project.name = json["displayName"].get<std::string>();
    if (json.contains("engineCompatibility") &&
        json["engineCompatibility"].is_object() &&
        json["engineCompatibility"].contains("targetVersion") &&
        json["engineCompatibility"]["targetVersion"].is_string())
        project.engineVersion =
            json["engineCompatibility"]["targetVersion"].get<std::string>();

    return true;
}

} // namespace

// ─── Pimpl Definition ────────────────────────────────────────────────────────

struct ProjectManager::Impl
{
    bool open = false;
    ProjectInfo current;
    std::function<void()> onProjectChanged;

    void NotifyProjectChanged() const
    {
        if (onProjectChanged)
        {
            onProjectChanged();
        }
    }
};

// ─── Method Stubs ─────────────────────────────────────────────────────────────

ProjectManager::ProjectManager()
    : m_impl(std::make_unique<Impl>())
{}

ProjectManager::~ProjectManager() = default;

bool ProjectManager::OpenProject(const std::string& path)
{
    const std::filesystem::path manifest = ResolveProjectManifest(path);
    if (manifest.empty())
    {
        return false;
    }

    std::error_code ec;
    const std::filesystem::path root = manifest.parent_path();
    if (!std::filesystem::exists(root, ec) ||
        !std::filesystem::is_directory(root, ec))
    {
        return false;
    }

    ProjectInfo next;
    next.rootPath = root.string();

    if (!ReadManifest(manifest, next))
    {
        return false;
    }

    m_impl->current = std::move(next);
    m_impl->open = true;
    m_impl->NotifyProjectChanged();
    return true;
}

void ProjectManager::CloseProject()
{
    if (!m_impl->open)
    {
        return;
    }

    m_impl->current = {};
    m_impl->open = false;
    m_impl->NotifyProjectChanged();
}

bool ProjectManager::IsProjectOpen() const noexcept
{
    return m_impl->open;
}

const ProjectInfo& ProjectManager::GetCurrentProject() const
{
    return m_impl->current;
}

void ProjectManager::SetOnProjectChanged(std::function<void()> cb)
{
    m_impl->onProjectChanged = std::move(cb);
}

} // namespace SagaEditor
