/// @file SagaProjectSystem.h
/// @brief Compatibility project helper used by package and publish workflows.

#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace SagaProduct
{

/// Stable project manifest consumed by the Saga product shell.
struct SagaProjectManifest
{
    std::string           projectId;    ///< Stable project identifier.
    std::string           displayName;  ///< User-visible project name.
    std::filesystem::path root;         ///< Absolute project root directory.
};

/// Result for project create/open operations with user-facing failure text.
struct SagaProjectResult
{
    bool                ok = false; ///< True when manifest and folders are usable.
    SagaProjectManifest manifest;
    std::string         error;
};

/// Recent project entry rendered by the product shell.
struct SagaRecentProject
{
    std::string           displayName; ///< Last known project name.
    std::filesystem::path root;        ///< Project root path.
    bool                  exists = false;
    std::string           warning;
};

/// Temporary compatibility adapter; the launcher uses SagaProjectCatalog.
class SagaProjectSystem
{
public:
    SagaProjectSystem();
    explicit SagaProjectSystem(std::filesystem::path recentProjectsPath);

    /// Create a project folder, manifest, and product-owned content roots.
    [[nodiscard]] SagaProjectResult CreateProject(
        const std::filesystem::path& parentDirectory,
        const std::string& displayName);

    /// Open and validate a project root or project manifest path.
    [[nodiscard]] SagaProjectResult OpenProject(
        const std::filesystem::path& path);

    /// Read recent projects from local product settings.
    [[nodiscard]] std::vector<SagaRecentProject> LoadRecentProjects() const;

    /// Insert or refresh a recent project entry.
    void RememberProject(const SagaProjectManifest& manifest);

    [[nodiscard]] const std::filesystem::path& RecentProjectsPath() const noexcept;

private:
    std::filesystem::path m_recentProjectsPath;
};

} // namespace SagaProduct
