/// @file SagaLauncherTargets.h
/// @brief Explicit launcher target resolution and executable catalog policy.

#pragma once

#include "Launcher/SagaLauncherModel.h"

#include <filesystem>
#include <optional>

namespace SagaProduct
{

class SagaExecutableCatalog
{
public:
    explicit SagaExecutableCatalog(std::filesystem::path productExecutable);
    [[nodiscard]] std::optional<std::filesystem::path> Resolve(SagaProcessTargetId id) const;

private:
    std::filesystem::path m_productExecutable;
};

struct SagaLauncherPathPolicy
{
    std::filesystem::path reportsRoot;
    std::filesystem::path evidenceRoot;

    [[nodiscard]] std::optional<std::filesystem::path> ActionReportDirectory(
        const SagaLauncherProjectSummary& project,
        SagaLauncherActionId action,
        std::string& error) const;
    [[nodiscard]] static bool IsContainedPath(const std::filesystem::path& root,
                                              const std::filesystem::path& candidate) noexcept;
};

class ISagaTargetResolver
{
public:
    virtual ~ISagaTargetResolver() = default;
    [[nodiscard]] virtual std::vector<SagaLauncherTargetSummary> ResolveTargets(
        const std::optional<SagaLauncherProjectSummary>& project) const = 0;
    [[nodiscard]] virtual std::vector<SagaLauncherAction> ResolveActions(
        const std::optional<SagaLauncherProjectSummary>& project) const = 0;
};

class SagaTargetResolver final : public ISagaTargetResolver
{
public:
    explicit SagaTargetResolver(SagaExecutableCatalog catalog,
                                std::filesystem::path runtimeBridgeAssembly = {});

    [[nodiscard]] std::vector<SagaLauncherTargetSummary> ResolveTargets(
        const std::optional<SagaLauncherProjectSummary>& project) const override;
    [[nodiscard]] std::vector<SagaLauncherAction> ResolveActions(
        const std::optional<SagaLauncherProjectSummary>& project) const override;

private:
    SagaExecutableCatalog m_catalog;
    std::filesystem::path m_runtimeBridgeAssembly;
};

namespace SagaLauncherTargetDiagnostics
{
inline constexpr const char* GenericRuntimeUnsupported =
    "Saga.Launcher.Target.GenericRuntimeUnsupported";
inline constexpr const char* WorldServerUnsupported = "Saga.Launcher.Target.WorldServerUnsupported";
inline constexpr const char* CloudCollaborationUnsupported =
    "Saga.Launcher.Target.CloudCollaborationUnsupported";
inline constexpr const char* ExecutableMissing = "Saga.Launcher.Target.ExecutableMissing";
} // namespace SagaLauncherTargetDiagnostics

} // namespace SagaProduct
