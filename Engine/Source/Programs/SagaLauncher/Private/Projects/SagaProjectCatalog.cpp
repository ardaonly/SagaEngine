/// @file SagaProjectCatalog.cpp

#include "Projects/SagaProjectCatalog.h"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <nlohmann/json.hpp>
#include <sstream>

namespace SagaProduct
{
namespace
{

constexpr std::size_t kMaximumRecentProjects = 12;

[[nodiscard]] SagaLauncherDiagnostic Diagnostic(const char* id,
                                                SagaLauncherDiagnosticSeverity severity,
                                                std::string message,
                                                std::filesystem::path path = {})
{
    SagaLauncherDiagnostic result;
    result.diagnosticId = id;
    result.severity = severity;
    result.message = std::move(message);
    if (!path.empty())
        result.path = std::move(path);
    return result;
}

[[nodiscard]] bool ContainsPrivateComponent(const std::filesystem::path& path)
{
    return std::any_of(path.begin(), path.end(), [](const auto& component) {
        return component == ".saga-private";
    });
}

[[nodiscard]] std::filesystem::path WeakCanonical(const std::filesystem::path& path)
{
    std::error_code error;
    auto canonical = std::filesystem::weakly_canonical(path, error);
    if (!error)
        return canonical;
    canonical = std::filesystem::absolute(path, error);
    return error ? std::filesystem::path{} : canonical.lexically_normal();
}

[[nodiscard]] std::string UtcNow()
{
    const auto now = std::chrono::system_clock::now();
    const std::time_t time = std::chrono::system_clock::to_time_t(now);
    std::tm utc{};
#if defined(_WIN32)
    gmtime_s(&utc, &time);
#else
    gmtime_r(&time, &utc);
#endif
    std::ostringstream stream;
    stream << std::put_time(&utc, "%Y-%m-%dT%H:%M:%SZ");
    return stream.str();
}

[[nodiscard]] bool ReadJson(const std::filesystem::path& path,
                            nlohmann::json& json,
                            std::string& error)
{
    std::ifstream input(path);
    if (!input.is_open())
    {
        error = "Cannot open manifest.";
        return false;
    }
    try
    {
        input >> json;
    }
    catch (const nlohmann::json::exception& exception)
    {
        error = std::string("Invalid JSON: ") + exception.what();
        return false;
    }
    if (!json.is_object())
    {
        error = "Manifest root must be a JSON object.";
        return false;
    }
    return true;
}

[[nodiscard]] SagaLauncherProjectSummary InvalidProject(const char* id,
                                                        std::string message,
                                                        const std::filesystem::path& path)
{
    SagaLauncherProjectSummary result;
    result.diagnostics.push_back(
        Diagnostic(id, SagaLauncherDiagnosticSeverity::Error, std::move(message), path));
    return result;
}

} // namespace

SagaLauncherProjectSummary SagaProjectCatalog::OpenProject(
    const std::filesystem::path& inputPath) const
{
    if (inputPath.empty() || ContainsPrivateComponent(inputPath))
    {
        return InvalidProject(SagaLauncherProjectDiagnostics::PrivatePathRejected,
                              "Private launcher paths are not accepted.",
                              inputPath);
    }

    const std::filesystem::path input = WeakCanonical(inputPath);
    if (input.empty() || !std::filesystem::exists(input))
    {
        return InvalidProject(SagaLauncherProjectDiagnostics::Missing,
                              "Project path does not exist.",
                              inputPath);
    }

    std::filesystem::path manifest;
    if (std::filesystem::is_regular_file(input))
    {
        if (input.extension() == ".sagaproj")
            manifest = input;
        else
        {
            return InvalidProject(SagaLauncherProjectDiagnostics::ManifestMissing,
                                  "Explicit project input must be a .sagaproj file.",
                                  input);
        }
    }
    else if (std::filesystem::is_directory(input))
    {
        std::vector<std::filesystem::path> candidates;
        std::error_code error;
        for (const auto& entry : std::filesystem::directory_iterator(input, error))
        {
            if (entry.is_regular_file() && entry.path().extension() == ".sagaproj")
                candidates.push_back(WeakCanonical(entry.path()));
        }
        if (error)
        {
            return InvalidProject(SagaLauncherProjectDiagnostics::ManifestInvalid,
                                  "Project directory cannot be enumerated: " + error.message(),
                                  input);
        }
        std::sort(candidates.begin(), candidates.end());
        if (candidates.size() > 1)
        {
            return InvalidProject(
                SagaLauncherProjectDiagnostics::ManifestAmbiguous,
                "Project directory contains more than one immediate .sagaproj manifest.",
                input);
        }
        if (candidates.size() == 1)
            manifest = candidates.front();
        else
        {
            return InvalidProject(SagaLauncherProjectDiagnostics::ManifestMissing,
                                  "Project directory contains no .sagaproj manifest.",
                                  input);
        }
    }
    else
    {
        return InvalidProject(SagaLauncherProjectDiagnostics::ManifestMissing,
                              "Project input is not a regular file or directory.",
                              input);
    }

    nlohmann::json json;
    std::string error;
    if (!ReadJson(manifest, json, error))
    {
        return InvalidProject(SagaLauncherProjectDiagnostics::ManifestInvalid,
                              std::move(error),
                              manifest);
    }

    if (!json.contains("schemaVersion") || !json["schemaVersion"].is_number_integer() ||
        json["schemaVersion"].get<int>() != 0)
    {
        return InvalidProject(SagaLauncherProjectDiagnostics::SchemaUnsupported,
                              ".sagaproj requires schemaVersion 0.",
                              manifest);
    }
    if (!json.contains("projectId") || !json["projectId"].is_string() ||
        json["projectId"].get<std::string>().empty() || !json.contains("displayName") ||
        !json["displayName"].is_string() || json["displayName"].get<std::string>().empty())
    {
        return InvalidProject(SagaLauncherProjectDiagnostics::ManifestInvalid,
                              "Manifest requires non-empty string projectId and displayName.",
                              manifest);
    }

    SagaLauncherProjectSummary result;
    result.projectId = json["projectId"].get<std::string>();
    result.displayName = json["displayName"].get<std::string>();
    result.canonicalManifestPath = manifest;
    result.canonicalRoot = WeakCanonical(manifest.parent_path());
    result.schemaVersion = 0;
    result.valid = true;
    return result;
}

SagaRecentProjectsStore::SagaRecentProjectsStore(std::filesystem::path storagePath)
    : m_storagePath(std::move(storagePath))
{}

std::vector<SagaLauncherRecentProject> SagaRecentProjectsStore::Load() const
{
    std::vector<SagaLauncherRecentProject> result;
    if (!std::filesystem::is_regular_file(m_storagePath))
        return result;

    nlohmann::json json;
    std::string error;
    if (!ReadJson(m_storagePath, json, error) || !json.contains("recentProjects") ||
        !json["recentProjects"].is_array())
        return result;

    if (json.value("schemaVersion", 0) != 1)
        return result;

    for (const auto& value : json["recentProjects"])
    {
        if (!value.is_object())
            continue;
        SagaLauncherRecentProject recent;
        recent.displayName = value.value("displayName", std::string{});
        recent.projectId = value.value("projectId", std::string{});
        recent.lastOpenedUtc = value.value("lastOpenedUtc", std::string{});
        recent.canonicalManifestPath = WeakCanonical(
            value.value("manifestPath", std::string{}));
        recent.canonicalRoot = WeakCanonical(value.value("root", std::string{}));
        recent.exists = std::filesystem::is_regular_file(recent.canonicalManifestPath);
        if (!recent.exists)
        {
            recent.diagnostics.push_back(
                Diagnostic(SagaLauncherProjectDiagnostics::RecentEntryMissing,
                           SagaLauncherDiagnosticSeverity::Warning,
                           "Recent project manifest is missing.",
                           recent.canonicalManifestPath));
        }
        result.push_back(std::move(recent));
        if (result.size() == kMaximumRecentProjects)
            break;
    }
    return result;
}

bool SagaRecentProjectsStore::Remember(const SagaLauncherProjectSummary& project,
                                       std::string& error)
{
    if (!project.valid || project.canonicalManifestPath.empty() ||
        ContainsPrivateComponent(m_storagePath) ||
        ContainsPrivateComponent(project.canonicalManifestPath))
    {
        error = "Invalid or private project/storage path.";
        return false;
    }

    auto recent = Load();
    recent.erase(std::remove_if(recent.begin(),
                                recent.end(),
                                [&](const auto& entry) {
                                    return entry.canonicalManifestPath ==
                                           project.canonicalManifestPath;
                                }),
                 recent.end());

    SagaLauncherRecentProject entry;
    entry.projectId = project.projectId;
    entry.displayName = project.displayName;
    entry.canonicalManifestPath = project.canonicalManifestPath;
    entry.canonicalRoot = project.canonicalRoot;
    entry.lastOpenedUtc = UtcNow();
    entry.exists = true;
    recent.insert(recent.begin(), std::move(entry));
    if (recent.size() > kMaximumRecentProjects)
        recent.resize(kMaximumRecentProjects);

    nlohmann::json json;
    json["schemaVersion"] = 1;
    json["recentProjects"] = nlohmann::json::array();
    for (const auto& item : recent)
    {
        json["recentProjects"].push_back({
            {"projectId", item.projectId},
            {"displayName", item.displayName},
            {"manifestPath", item.canonicalManifestPath.string()},
            {"root", item.canonicalRoot.string()},
            {"lastOpenedUtc", item.lastOpenedUtc},
        });
    }

    std::error_code filesystemError;
    std::filesystem::create_directories(m_storagePath.parent_path(), filesystemError);
    if (filesystemError)
    {
        error = "Cannot create recent-project settings directory: " + filesystemError.message();
        return false;
    }
    std::ofstream output(m_storagePath, std::ios::trunc);
    if (!output.is_open())
    {
        error = "Cannot write recent-project settings.";
        return false;
    }
    output << json.dump(2) << '\n';
    if (!output.good())
    {
        error = "Cannot finish recent-project settings write.";
        return false;
    }
    return true;
}

const std::filesystem::path& SagaRecentProjectsStore::StoragePath() const noexcept
{
    return m_storagePath;
}

} // namespace SagaProduct
