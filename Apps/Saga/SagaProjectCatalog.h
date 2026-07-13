/// @file SagaProjectCatalog.h
/// @brief Canonical .sagaproj discovery and launcher recent-project storage.

#pragma once

#include "SagaLauncherModel.h"

#include <filesystem>
#include <vector>

namespace SagaProduct
{

namespace SagaLauncherProjectDiagnostics
{
inline constexpr const char* Missing = "Saga.Launcher.Project.Missing";
inline constexpr const char* ManifestMissing = "Saga.Launcher.Project.ManifestMissing";
inline constexpr const char* ManifestAmbiguous = "Saga.Launcher.Project.ManifestAmbiguous";
inline constexpr const char* ManifestInvalid = "Saga.Launcher.Project.ManifestInvalid";
inline constexpr const char* SchemaUnsupported = "Saga.Launcher.Project.SchemaUnsupported";
inline constexpr const char* RecentEntryMissing = "Saga.Launcher.Project.RecentEntryMissing";
inline constexpr const char* PrivatePathRejected = "Saga.Launcher.Path.PrivateComponentRejected";
} // namespace SagaLauncherProjectDiagnostics

class ISagaProjectCatalog
{
public:
    virtual ~ISagaProjectCatalog() = default;
    [[nodiscard]] virtual SagaLauncherProjectSummary OpenProject(
        const std::filesystem::path& path) const = 0;
};

class SagaProjectCatalog final : public ISagaProjectCatalog
{
public:
    [[nodiscard]] SagaLauncherProjectSummary OpenProject(
        const std::filesystem::path& path) const override;
};

class ISagaRecentProjectsStore
{
public:
    virtual ~ISagaRecentProjectsStore() = default;
    [[nodiscard]] virtual std::vector<SagaLauncherRecentProject> Load() const = 0;
    virtual bool Remember(const SagaLauncherProjectSummary& project, std::string& error) = 0;
};

class SagaRecentProjectsStore final : public ISagaRecentProjectsStore
{
public:
    explicit SagaRecentProjectsStore(std::filesystem::path storagePath);

    [[nodiscard]] std::vector<SagaLauncherRecentProject> Load() const override;
    bool Remember(const SagaLauncherProjectSummary& project, std::string& error) override;
    [[nodiscard]] const std::filesystem::path& StoragePath() const noexcept;

private:
    std::filesystem::path m_storagePath;
};

} // namespace SagaProduct
