/// @file ProjectManager.cpp
/// @brief Editor-local project session state.

#include "SagaEditor/Project/ProjectManager.h"

#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <utility>

namespace SagaEditor
{
namespace
{

constexpr const char* kProjectManifestFile = "saga.project.json";

[[nodiscard]] std::filesystem::path ResolveProjectRoot(
    const std::filesystem::path& input)
{
    if (input.empty())
    {
        return {};
    }

    std::error_code ec;
    if (std::filesystem::is_regular_file(input, ec))
    {
        return std::filesystem::absolute(input.parent_path(), ec);
    }

    return std::filesystem::absolute(input, ec);
}

[[nodiscard]] std::string DirectoryDisplayName(
    const std::filesystem::path& root)
{
    const std::string filename = root.filename().string();
    return filename.empty() ? root.string() : filename;
}

[[nodiscard]] bool ReadManifest(
    const std::filesystem::path& manifestPath,
    ProjectInfo& project)
{
    if (!std::filesystem::exists(manifestPath))
    {
        return true;
    }

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

    if (json.contains("displayName") && json["displayName"].is_string())
    {
        project.name = json["displayName"].get<std::string>();
    }
    else if (json.contains("name") && json["name"].is_string())
    {
        project.name = json["name"].get<std::string>();
    }

    if (json.contains("engineVersion") && json["engineVersion"].is_string())
    {
        project.engineVersion = json["engineVersion"].get<std::string>();
    }
    else if (json.contains("engine") && json["engine"].is_string())
    {
        project.engineVersion = json["engine"].get<std::string>();
    }

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
    const std::filesystem::path root = ResolveProjectRoot(path);
    if (root.empty())
    {
        return false;
    }

    std::error_code ec;
    if (!std::filesystem::exists(root, ec) ||
        !std::filesystem::is_directory(root, ec))
    {
        return false;
    }

    ProjectInfo next;
    next.rootPath = root.string();
    next.name = DirectoryDisplayName(root);

    if (!ReadManifest(root / kProjectManifestFile, next))
    {
        return false;
    }

    if (next.name.empty())
    {
        next.name = DirectoryDisplayName(root);
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
