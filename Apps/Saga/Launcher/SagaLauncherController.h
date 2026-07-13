/// @file SagaLauncherController.h
/// @brief Non-widget controller and cancellable launcher action execution.

#pragma once

#include "Launcher/SagaLauncherModel.h"
#include "SagaLauncherReports.h"
#include "SagaLauncherTargets.h"
#include "SagaProjectCatalog.h"

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <stop_token>
#include <thread>

namespace SagaProduct
{

class ISagaLauncherActionExecutor
{
public:
    virtual ~ISagaLauncherActionExecutor() = default;
    [[nodiscard]] virtual SagaLauncherActionResult Execute(
        SagaLauncherActionId action,
        const SagaLauncherProjectSummary& project,
        std::stop_token stopToken) = 0;
};

class SagaLauncherActionExecutor final : public ISagaLauncherActionExecutor
{
public:
    SagaLauncherActionExecutor(SagaExecutableCatalog catalog,
                               SagaLauncherPathPolicy pathPolicy,
                               std::filesystem::path runtimeBridgeAssembly);
    [[nodiscard]] SagaLauncherActionResult Execute(SagaLauncherActionId action,
                                                   const SagaLauncherProjectSummary& project,
                                                   std::stop_token stopToken) override;

private:
    SagaExecutableCatalog m_catalog;
    SagaLauncherPathPolicy m_pathPolicy;
    std::filesystem::path m_runtimeBridgeAssembly;
};

class ISagaLauncherTaskRunner
{
public:
    virtual ~ISagaLauncherTaskRunner() = default;
    virtual bool Start(std::function<void(std::stop_token)> task) = 0;
    virtual bool RequestStop() = 0;
    [[nodiscard]] virtual bool Running() const noexcept = 0;
};

class SagaLauncherTaskRunner final : public ISagaLauncherTaskRunner
{
public:
    ~SagaLauncherTaskRunner() override;
    bool Start(std::function<void(std::stop_token)> task) override;
    bool RequestStop() override;
    [[nodiscard]] bool Running() const noexcept override;

private:
    mutable std::mutex m_mutex;
    std::jthread m_thread;
    std::atomic_bool m_running{false};
};

struct SagaLauncherCommandResult
{
    bool accepted = false;
    SagaLauncherActionStatus status = SagaLauncherActionStatus::Unknown;
    std::vector<SagaLauncherDiagnostic> diagnostics;
};

class SagaLauncherController final
{
public:
    using StateChangedCallback = std::function<void(const SagaLauncherState&)>;

    SagaLauncherController(std::unique_ptr<ISagaProjectCatalog> projectCatalog,
                           std::unique_ptr<ISagaRecentProjectsStore> recentStore,
                           std::unique_ptr<ISagaTargetResolver> targetResolver,
                           std::unique_ptr<ISagaLauncherActionExecutor> executor,
                           std::unique_ptr<ISagaReportCatalog> reportCatalog,
                           std::unique_ptr<ISagaDistributionStatusReader> distributionReader,
                           std::unique_ptr<ISagaLauncherTaskRunner> taskRunner);
    ~SagaLauncherController();

    void LoadInitialState();
    [[nodiscard]] SagaLauncherCommandResult OpenProject(const std::filesystem::path& path);
    void RefreshRecentProjects();
    [[nodiscard]] SagaLauncherCommandResult ValidateProject();
    void ResolveTargets();
    [[nodiscard]] SagaLauncherCommandResult RunAction(SagaLauncherActionId action);
    [[nodiscard]] SagaLauncherCommandResult CancelAction(SagaLauncherActionId action);
    void RefreshReports();
    void RefreshDistributionStatus();
    [[nodiscard]] SagaLauncherState GetState() const;
    void SetStateChangedCallback(StateChangedCallback callback);

private:
    void ResolveTargetsLocked();
    void RefreshReportsLocked();
    void RefreshDistributionStatusLocked();
    void CompleteAction(SagaLauncherActionResult result);
    void EmitStateChanged();

    mutable std::mutex m_mutex;
    SagaLauncherState m_state;
    StateChangedCallback m_callback;
    std::unique_ptr<ISagaProjectCatalog> m_projectCatalog;
    std::unique_ptr<ISagaRecentProjectsStore> m_recentStore;
    std::unique_ptr<ISagaTargetResolver> m_targetResolver;
    std::unique_ptr<ISagaLauncherActionExecutor> m_executor;
    std::unique_ptr<ISagaReportCatalog> m_reportCatalog;
    std::unique_ptr<ISagaDistributionStatusReader> m_distributionReader;
    std::unique_ptr<ISagaLauncherTaskRunner> m_taskRunner;
};

namespace SagaLauncherActionDiagnostics
{
inline constexpr const char* AlreadyRunning = "Saga.Launcher.Action.AlreadyRunning";
inline constexpr const char* NotRunnable = "Saga.Launcher.Action.NotRunnable";
inline constexpr const char* NoSelectedProject = "Saga.Launcher.Action.NoSelectedProject";
inline constexpr const char* ReportPathInvalid = "Saga.Launcher.Action.ReportPathInvalid";
} // namespace SagaLauncherActionDiagnostics

} // namespace SagaProduct
