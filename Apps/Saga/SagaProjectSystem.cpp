/// @file SagaProjectSystem.cpp
/// @brief Product project manifest and recent project persistence implementation.

#include "SagaProjectSystem.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <random>
#include <sstream>

namespace SagaProduct
{
namespace
{

constexpr const char* kProjectManifestFile = "saga.project.json";
constexpr const char* kDefaultSdeRoot = ".sde";

[[nodiscard]] std::filesystem::path DefaultRecentProjectsPath()
{
    if (const char* home = std::getenv("HOME"))
    {
        return std::filesystem::path(home) / ".config" / "Saga" /
            "recent_projects.json";
    }
    return std::filesystem::temp_directory_path() / "Saga" /
        "recent_projects.json";
}

[[nodiscard]] std::string SanitizeProjectId(const std::string& displayName)
{
    std::string id;
    id.reserve(displayName.size());
    for (const char c : displayName)
    {
        if (std::isalnum(static_cast<unsigned char>(c)))
        {
            id.push_back(static_cast<char>(
                std::tolower(static_cast<unsigned char>(c))));
        }
        else if (!id.empty() && id.back() != '.')
        {
            id.push_back('.');
        }
    }
    while (!id.empty() && id.back() == '.')
    {
        id.pop_back();
    }
    return id.empty() ? "untitled" : id;
}

[[nodiscard]] std::filesystem::path ResolveProjectRoot(
    const std::filesystem::path& path)
{
    if (path.filename() == kProjectManifestFile)
    {
        return path.parent_path();
    }
    return path;
}

void WriteTextFile(const std::filesystem::path& path, const std::string& text)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::trunc);
    out << text;
}

void WriteMinimumSdeWorkspace(const std::filesystem::path& root,
                              const SagaProjectManifest& manifest)
{
    std::ostringstream workspace;
    workspace
        << "sde version 1\n\n"
        << "package Saga.Workspace\n\n"
        << "model SagaWorkspace version 1 {\n"
        << "  field displayName Text required\n"
        << "  field editorProfile Text required\n"
        << "  field runtimeRole Text required\n"
        << "  field serverRole Text required\n"
        << "}\n\n"
        << "instance SagaWorkspace " << manifest.projectId << " {\n"
        << "  displayName = \"" << manifest.displayName << "\"\n"
        << "  editorProfile = \"saga.profile.basic\"\n"
        << "  runtimeRole = \"SagaRuntime\"\n"
        << "  serverRole = \"SagaServer\"\n"
        << "}\n";
    WriteTextFile(root / "source" / "workspace.sde", workspace.str());

    WriteTextFile(root / "source" / "editor" / "profiles.sde", R"(sde version 1

package Saga.Editor.Customization

model EditorProfile version 1 {
  field displayName Text required
  field description Text optional
  field layoutPresetId Text required
  field shortcutMapId Text required
  field defaultPanels List<Text> optional
  field defaultToolbarCommands List<Text> optional
  field visibleToolCommands List<Text> optional
  field shortcutBindings List<Text> optional
  field showMenuBar Boolean optional
  field showStatusBar Boolean optional
  field showMainToolbar Boolean optional
  field exposesGraphEditor Boolean optional
  field exposesProfiler Boolean optional
  field exposesConsole Boolean optional
  field exposesAssetBrowser Boolean optional
}

instance EditorProfile saga.profile.project_basic {
  displayName = "Project Basic"
  description = "Project-authored guided workspace."
  layoutPresetId = "basic"
  shortcutMapId = "saga.shortcuts.basic"
  defaultPanels = ["saga.panel.production_dashboard", "saga.panel.hierarchy", "saga.panel.viewport", "saga.panel.console"]
  defaultToolbarCommands = ["saga.command.world.play", "saga.command.world.stop", "saga.command.file.save"]
  visibleToolCommands = ["saga.command.world.play", "saga.command.world.stop", "saga.command.file.save"]
  shortcutBindings = ["Ctrl+S|saga.command.file.save|Ctrl+S"]
  showMenuBar = true
  showStatusBar = true
  showMainToolbar = true
  exposesGraphEditor = true
  exposesProfiler = false
  exposesConsole = true
  exposesAssetBrowser = false
}
)");
}

[[nodiscard]] std::filesystem::path ResolveManifestSdeRoot(
    const std::filesystem::path& projectRoot,
    const nlohmann::json& manifest)
{
    const std::string configured =
        manifest.value("sdeRoot", std::string{kDefaultSdeRoot});
    const std::filesystem::path candidate =
        std::filesystem::path(configured).is_absolute()
            ? std::filesystem::path(configured)
            : projectRoot / configured;

    if (std::filesystem::exists(candidate / "source") ||
        std::filesystem::exists(candidate / "schemas") ||
        std::filesystem::exists(candidate / "data"))
    {
        return candidate;
    }

    // Compatibility for projects created before the .sde convention.
    return projectRoot;
}

[[nodiscard]] std::string RandomRoomSuffix()
{
    static constexpr char kAlphabet[] = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";
    std::mt19937 rng(static_cast<std::mt19937::result_type>(
        std::chrono::steady_clock::now().time_since_epoch().count()));
    std::uniform_int_distribution<std::size_t> dist(0, sizeof(kAlphabet) - 2);

    std::string suffix;
    suffix.reserve(4);
    for (int i = 0; i < 4; ++i)
    {
        suffix.push_back(kAlphabet[dist(rng)]);
    }
    return suffix;
}

[[nodiscard]] bool IsRoomCodeCharacter(char c) noexcept
{
    return std::isalnum(static_cast<unsigned char>(c)) || c == '-';
}

} // namespace

SagaProjectSystem::SagaProjectSystem()
    : m_recentProjectsPath(DefaultRecentProjectsPath())
{}

SagaProjectSystem::SagaProjectSystem(std::filesystem::path recentProjectsPath)
    : m_recentProjectsPath(std::move(recentProjectsPath))
{}

SagaProjectResult SagaProjectSystem::CreateProject(
    const std::filesystem::path& parentDirectory,
    const std::string& displayName)
{
    SagaProjectResult result;
    if (displayName.empty())
    {
        result.error = "Project name is required.";
        return result;
    }
    if (parentDirectory.empty() || !std::filesystem::exists(parentDirectory))
    {
        result.error = "Project parent directory does not exist.";
        return result;
    }

    const std::string projectId = SanitizeProjectId(displayName);
    const std::filesystem::path root =
        std::filesystem::absolute(parentDirectory / displayName);
    if (std::filesystem::exists(root / kProjectManifestFile))
    {
        result.error = "Project already exists: " + root.string();
        return result;
    }

    std::error_code ec;
    const std::filesystem::path sdeRoot = root / kDefaultSdeRoot;
    std::filesystem::create_directories(sdeRoot / "source" / "editor", ec);
    std::filesystem::create_directories(sdeRoot / "artifacts", ec);
    std::filesystem::create_directories(sdeRoot / "cache", ec);
    std::filesystem::create_directories(root / "Assets", ec);
    if (ec)
    {
        result.error = "Cannot create project folders: " + ec.message();
        return result;
    }

    SagaProjectManifest manifest;
    manifest.projectId = projectId;
    manifest.displayName = displayName;
    manifest.root = root;
    manifest.sdeRoot = sdeRoot;

    nlohmann::json json;
    json["schemaVersion"] = 1;
    json["projectId"] = manifest.projectId;
    json["displayName"] = manifest.displayName;
    json["sdeRoot"] = kDefaultSdeRoot;
    WriteTextFile(root / kProjectManifestFile, json.dump(2));
    WriteMinimumSdeWorkspace(sdeRoot, manifest);

    result.ok = true;
    result.manifest = std::move(manifest);
    RememberProject(result.manifest);
    return result;
}

SagaProjectResult SagaProjectSystem::OpenProject(
    const std::filesystem::path& path)
{
    SagaProjectResult result;
    const std::filesystem::path root =
        std::filesystem::absolute(ResolveProjectRoot(path));
    const std::filesystem::path manifestPath = root / kProjectManifestFile;

    if (!std::filesystem::exists(manifestPath))
    {
        result.error = "Project manifest is missing: " + manifestPath.string();
        return result;
    }

    std::ifstream input(manifestPath);
    if (!input.is_open())
    {
        result.error = "Cannot open project manifest: " + manifestPath.string();
        return result;
    }

    nlohmann::json json;
    try
    {
        input >> json;
    }
    catch (const nlohmann::json::exception& e)
    {
        result.error = std::string("Project manifest JSON is invalid: ") +
            e.what();
        return result;
    }

    if (!json.contains("projectId") || !json["projectId"].is_string() ||
        !json.contains("displayName") || !json["displayName"].is_string())
    {
        result.error =
            "Project manifest must contain string projectId and displayName.";
        return result;
    }
    const std::filesystem::path sdeRoot = ResolveManifestSdeRoot(root, json);
    if (!std::filesystem::exists(sdeRoot / "source") &&
        (!std::filesystem::exists(sdeRoot / "schemas") ||
         !std::filesystem::exists(sdeRoot / "data")))
    {
        result.error =
            "Project must contain .sde/source or legacy .sde/schemas and .sde/data.";
        return result;
    }

    result.ok = true;
    result.manifest.projectId = json["projectId"].get<std::string>();
    result.manifest.displayName = json["displayName"].get<std::string>();
    result.manifest.root = root;
    result.manifest.sdeRoot = sdeRoot;
    RememberProject(result.manifest);
    return result;
}

std::vector<SagaRecentProject> SagaProjectSystem::LoadRecentProjects() const
{
    std::vector<SagaRecentProject> projects;
    if (!std::filesystem::exists(m_recentProjectsPath))
    {
        return projects;
    }

    std::ifstream input(m_recentProjectsPath);
    if (!input.is_open())
    {
        return projects;
    }

    nlohmann::json json;
    try
    {
        input >> json;
    }
    catch (const nlohmann::json::exception&)
    {
        return projects;
    }

    if (!json.contains("recentProjects") || !json["recentProjects"].is_array())
    {
        return projects;
    }

    for (const auto& item : json["recentProjects"])
    {
        if (!item.is_object() || !item.contains("root") ||
            !item["root"].is_string())
        {
            continue;
        }

        SagaRecentProject project;
        project.root = item["root"].get<std::string>();
        project.displayName = item.contains("displayName") &&
                item["displayName"].is_string()
            ? item["displayName"].get<std::string>()
            : project.root.filename().string();
        project.exists = std::filesystem::exists(project.root / kProjectManifestFile);
        if (!project.exists)
        {
            project.warning = "Project manifest is missing.";
        }
        projects.push_back(std::move(project));
    }
    return projects;
}

void SagaProjectSystem::RememberProject(const SagaProjectManifest& manifest)
{
    std::vector<SagaRecentProject> projects = LoadRecentProjects();
    projects.erase(std::remove_if(projects.begin(), projects.end(),
        [&manifest](const SagaRecentProject& recent)
        {
            std::error_code ec;
            return std::filesystem::equivalent(recent.root, manifest.root, ec);
        }), projects.end());

    projects.insert(projects.begin(), {
        manifest.displayName,
        manifest.root,
        true,
        {}
    });
    if (projects.size() > 12)
    {
        projects.resize(12);
    }

    nlohmann::json json;
    json["recentProjects"] = nlohmann::json::array();
    for (const SagaRecentProject& project : projects)
    {
        json["recentProjects"].push_back({
            { "displayName", project.displayName },
            { "root", project.root.string() }
        });
    }

    std::filesystem::create_directories(m_recentProjectsPath.parent_path());
    WriteTextFile(m_recentProjectsPath, json.dump(2));
}

SagaLocalSession SagaProjectSystem::StartLocalSession() const
{
    SagaLocalSession session;
    session.hosting = true;
    session.roomCode = "LOCAL-" + RandomRoomSuffix();
    session.status = "Hosting local session. Remote networking is unavailable.";
    return session;
}

std::optional<std::string> SagaProjectSystem::ValidateRoomCode(
    const std::string& roomCode)
{
    if (roomCode.empty())
    {
        return "Room code is required.";
    }
    if (roomCode.size() < 8 || roomCode.size() > 20)
    {
        return "Room code must be 8 to 20 characters.";
    }
    if (std::find(roomCode.begin(), roomCode.end(), '-') == roomCode.end())
    {
        return "Room code must contain a prefix and code separated by '-'.";
    }
    if (!std::all_of(roomCode.begin(), roomCode.end(), IsRoomCodeCharacter))
    {
        return "Room code may only contain letters, numbers, and '-'.";
    }
    return std::nullopt;
}

const std::filesystem::path& SagaProjectSystem::RecentProjectsPath() const noexcept
{
    return m_recentProjectsPath;
}

} // namespace SagaProduct
