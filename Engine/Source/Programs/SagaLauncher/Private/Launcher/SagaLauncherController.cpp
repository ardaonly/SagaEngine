/// @file SagaLauncherController.cpp

#include "Launcher/SagaLauncherController.h"

#include "FirstPlayable/FirstPlayableWorkflow.h"
#include "Processes/SagaProcessService.h"

#include <algorithm>
#include <sstream>
#include <utility>

namespace SagaProduct
{
namespace
{

[[nodiscard]] SagaLauncherDiagnostic Diagnostic(
    const char* id,
    SagaLauncherDiagnosticSeverity severity,
    std::string message,
    std::optional<SagaLauncherActionId> action = std::nullopt,
    std::filesystem::path path = {})
{
    SagaLauncherDiagnostic result;
    result.diagnosticId = id;
    result.severity = severity;
    result.message = std::move(message);
    result.actionId = action;
    if (!path.empty())
        result.path = std::move(path);
    return result;
}

[[nodiscard]] SagaLauncherActionStatus MapProcessStatus(
    SagaProcessExitClassification classification)
{
    switch (classification)
    {
    case SagaProcessExitClassification::Succeeded:
    case SagaProcessExitClassification::Detached:
        return SagaLauncherActionStatus::Passed;
    case SagaProcessExitClassification::Cancelled:
        return SagaLauncherActionStatus::Cancelled;
    case SagaProcessExitClassification::NotStarted:
    case SagaProcessExitClassification::InvalidRequest:
        return SagaLauncherActionStatus::Blocked;
    case SagaProcessExitClassification::Failed:
    case SagaProcessExitClassification::Crashed:
    case SagaProcessExitClassification::TimedOut:
        return SagaLauncherActionStatus::Failed;
    }
    return SagaLauncherActionStatus::Unknown;
}

[[nodiscard]] std::filesystem::path FirstPlayableBridgeFrom(const std::filesystem::path& configured)
{
    std::error_code error;
    return std::filesystem::weakly_canonical(configured, error);
}

} // namespace

SagaLauncherActionExecutor::SagaLauncherActionExecutor(SagaExecutableCatalog catalog,
                                                       SagaLauncherPathPolicy pathPolicy,
                                                       std::filesystem::path runtimeBridgeAssembly)
    : m_catalog(std::move(catalog)), m_pathPolicy(std::move(pathPolicy)),
      m_runtimeBridgeAssembly(std::move(runtimeBridgeAssembly))
{}

SagaLauncherActionResult SagaLauncherActionExecutor::Execute(
    SagaLauncherActionId action,
    const SagaLauncherProjectSummary& project,
    std::stop_token stopToken)
{
    SagaLauncherActionResult result;
    result.actionId = action;
    const auto startedAt = std::chrono::steady_clock::now();
    std::string pathError;
    const auto outputDirectory = m_pathPolicy.ActionReportDirectory(project, action, pathError);
    if (!outputDirectory.has_value())
    {
        result.status = SagaLauncherActionStatus::Blocked;
        result.diagnostics.push_back(Diagnostic(SagaLauncherActionDiagnostics::ReportPathInvalid,
                                                SagaLauncherDiagnosticSeverity::Error,
                                                pathError,
                                                action));
        return result;
    }
    std::error_code filesystemError;
    std::filesystem::create_directories(*outputDirectory, filesystemError);
    if (filesystemError)
    {
        result.status = SagaLauncherActionStatus::Blocked;
        result.diagnostics.push_back(
            Diagnostic(SagaLauncherActionDiagnostics::ReportPathInvalid,
                       SagaLauncherDiagnosticSeverity::Error,
                       "Cannot create launcher report directory: " + filesystemError.message(),
                       action,
                       *outputDirectory));
        return result;
    }

    if (action == SagaLauncherActionId::FirstPlayableCheck)
    {
        const auto runtime = m_catalog.Resolve(SagaProcessTargetId::Runtime);
        const auto sagaScript = m_catalog.Resolve(SagaProcessTargetId::SagaScript);
        const auto bridge = FirstPlayableBridgeFrom(m_runtimeBridgeAssembly);
        if (!runtime.has_value() || !sagaScript.has_value() ||
            !std::filesystem::is_regular_file(bridge))
        {
            result.status = SagaLauncherActionStatus::MissingInput;
            result.diagnostics.push_back(Diagnostic(
                SagaLauncherTargetDiagnostics::ExecutableMissing,
                SagaLauncherDiagnosticSeverity::Error,
                "First-playable requires SagaRuntime, sagascript, and the Runtime Bridge assembly.",
                action));
            return result;
        }
        FirstPlayableWorkflowRequest request;
        request.runtime.projectManifest = project.canonicalManifestPath;
        request.runtime.outputDirectory = *outputDirectory;
        request.runtime.runtimeExecutable = *runtime;
        request.runtime.sagaScriptExecutable = *sagaScript;
        request.runtime.runtimeBridgeAssembly = bridge;
        request.runtime.timeout = std::chrono::seconds(30);
        request.runtime.stopToken = stopToken;
        request.summaryPath = *outputDirectory / "first_playable_summary.json";
        std::ostringstream out;
        std::ostringstream err;
        const auto workflow = RunFirstPlayableWorkflow(request, out, err);
        result.started = workflow.runtime.prepared;
        result.cancelled = stopToken.stop_requested();
        result.status = result.cancelled ? SagaLauncherActionStatus::Cancelled
                        : workflow.gate.status == FirstPlayableGateStatus::Accepted
                            ? SagaLauncherActionStatus::Passed
                        : workflow.gate.status ==
                                FirstPlayableGateStatus::AcceptedWithManualEvidencePending
                            ? SagaLauncherActionStatus::PassedWithLimitations
                        : workflow.gate.status == FirstPlayableGateStatus::Incomplete
                            ? SagaLauncherActionStatus::MissingInput
                            : SagaLauncherActionStatus::Failed;
        result.processExitClassification =
            result.cancelled ? SagaProcessExitClassification::Cancelled
            : result.status == SagaLauncherActionStatus::Passed ||
                    result.status == SagaLauncherActionStatus::PassedWithLimitations
                ? SagaProcessExitClassification::Succeeded
                : SagaProcessExitClassification::Failed;
        result.standardOutputEvaluation = out.str();
        result.standardErrorEvaluation = err.str();
        for (const auto& [type, path] : {std::pair{"first-playable-summary", workflow.summaryPath},
                                         std::pair{"first-playable-gate", workflow.gatePath}})
        {
            SagaLauncherReportReference report;
            report.reportType = type;
            report.owner = "Saga";
            report.path = path;
            report.exists = std::filesystem::is_regular_file(path);
            report.schemaVersion = 1;
            report.rawStatus = ToString(workflow.gate.status);
            report.mappedStatus = result.status;
            result.reportReferences.push_back(std::move(report));
        }
    }
    else
    {
        SagaProductProcessRequest request;
        request.stopToken = stopToken;
        request.timeout = std::chrono::seconds(30);
        request.workingDirectory = project.canonicalRoot;
        if (action == SagaLauncherActionId::OpenEditor)
        {
            request.target = SagaProcessTargetId::Editor;
            request.mode = SagaProcessExecutionMode::Detached;
            request.arguments = {"--workspace", project.canonicalRoot.string()};
        }
        else if (action == SagaLauncherActionId::ValidateProject)
        {
            request.target = SagaProcessTargetId::SagaProject;
            request.arguments = {"validate",
                                 "--project",
                                 project.canonicalManifestPath.string(),
                                 "--out",
                                 (*outputDirectory / "project-validation.json").string()};
        }
        else if (action == SagaLauncherActionId::RuntimeStarterArenaSmoke)
        {
            request.target = SagaProcessTargetId::Runtime;
            request.arguments = {"--headless",
                                 "--project",
                                 project.canonicalManifestPath.string(),
                                 "--starter-arena-smoke",
                                 "--smoke-frames",
                                 "30",
                                 "--fixed-dt",
                                 "0.016",
                                 "--smoke-report-out",
                                 (*outputDirectory / "runtime-smoke.json").string()};
        }
        else if (action == SagaLauncherActionId::RuntimeStarterArenaPlayable)
        {
            request.target = SagaProcessTargetId::Runtime;
            request.arguments = {
                "--project",
                project.canonicalManifestPath.string(),
                "--starter-arena-playable",
                "--playable-frames",
                "30",
                "--playable-input-source",
                "synthetic",
                "--playable-input-script",
                (project.canonicalRoot / "Input" / "playable.synthetic-input.json").string(),
                "--fixed-dt",
                "0.016",
                "--playable-report-out",
                (*outputDirectory / "runtime-playable.json").string()};
        }
        else
        {
            result.status = SagaLauncherActionStatus::Unsupported;
            result.diagnostics.push_back(
                Diagnostic(SagaLauncherActionDiagnostics::NotRunnable,
                           SagaLauncherDiagnosticSeverity::Error,
                           "The typed launcher action has no executable contract.",
                           action));
            return result;
        }

        const auto executable = m_catalog.Resolve(request.target);
        if (!executable.has_value())
        {
            result.status = SagaLauncherActionStatus::MissingInput;
            result.diagnostics.push_back(
                Diagnostic(SagaLauncherTargetDiagnostics::ExecutableMissing,
                           SagaLauncherDiagnosticSeverity::Error,
                           "Required allowlisted executable is missing.",
                           action));
            return result;
        }
        request.executable = *executable;
        SagaProcessService processService;
        const auto process = processService.Run(request);
        result.started = process.started;
        result.cancelled = process.cancelled;
        if (process.started || process.classification == SagaProcessExitClassification::Detached)
            result.exitCode = process.exitCode;
        result.processExitClassification = process.classification;
        result.status = MapProcessStatus(process.classification);
        result.standardOutputEvaluation = process.standardOutput;
        result.standardErrorEvaluation = process.standardError;
        if (!process.error.empty())
            result.diagnostics.push_back(Diagnostic("Saga.Launcher.Action.ProcessFailed",
                                                    SagaLauncherDiagnosticSeverity::Error,
                                                    process.error,
                                                    action,
                                                    *executable));
    }
    result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - startedAt);
    return result;
}

SagaLauncherTaskRunner::~SagaLauncherTaskRunner()
{
    RequestStop();
}

bool SagaLauncherTaskRunner::Start(std::function<void(std::stop_token)> task)
{
    std::lock_guard lock(m_mutex);
    if (m_running.exchange(true))
        return false;
    if (m_thread.joinable())
        m_thread.join();
    m_thread = std::jthread([this, task = std::move(task)](std::stop_token token) {
        task(token);
        m_running.store(false);
    });
    return true;
}

bool SagaLauncherTaskRunner::RequestStop()
{
    std::lock_guard lock(m_mutex);
    return m_thread.joinable() && m_thread.request_stop();
}

bool SagaLauncherTaskRunner::Running() const noexcept
{
    return m_running.load();
}

SagaLauncherController::SagaLauncherController(
    std::unique_ptr<ISagaProjectCatalog> projectCatalog,
    std::unique_ptr<ISagaRecentProjectsStore> recentStore,
    std::unique_ptr<ISagaTargetResolver> targetResolver,
    std::unique_ptr<ISagaLauncherActionExecutor> executor,
    std::unique_ptr<ISagaReportCatalog> reportCatalog,
    std::unique_ptr<ISagaDistributionStatusReader> distributionReader,
    std::unique_ptr<ISagaLauncherTaskRunner> taskRunner)
    : m_projectCatalog(std::move(projectCatalog)), m_recentStore(std::move(recentStore)),
      m_targetResolver(std::move(targetResolver)), m_executor(std::move(executor)),
      m_reportCatalog(std::move(reportCatalog)),
      m_distributionReader(std::move(distributionReader)), m_taskRunner(std::move(taskRunner))
{}

SagaLauncherController::~SagaLauncherController()
{
    if (m_taskRunner)
        m_taskRunner->RequestStop();
    m_taskRunner.reset();
}

void SagaLauncherController::LoadInitialState()
{
    {
        std::lock_guard lock(m_mutex);
        m_state = {};
        m_state.recentProjects = m_recentStore->Load();
        ResolveTargetsLocked();
        RefreshReportsLocked();
        RefreshDistributionStatusLocked();
        ++m_state.revision;
    }
    EmitStateChanged();
}

SagaLauncherCommandResult SagaLauncherController::OpenProject(const std::filesystem::path& path)
{
    SagaLauncherCommandResult command;
    const auto project = m_projectCatalog->OpenProject(path);
    command.accepted = project.valid;
    command.status = project.valid ? SagaLauncherActionStatus::Passed
                                   : SagaLauncherActionStatus::Failed;
    command.diagnostics = project.diagnostics;
    {
        std::lock_guard lock(m_mutex);
        m_state.diagnostics = project.diagnostics;
        if (project.valid)
        {
            m_state.selectedProject = project;
            std::string error;
            if (!m_recentStore->Remember(project, error))
                m_state.diagnostics.push_back(Diagnostic("Saga.Launcher.RecentProjects.WriteFailed",
                                                         SagaLauncherDiagnosticSeverity::Warning,
                                                         error));
            m_state.recentProjects = m_recentStore->Load();
            m_state.workspace.workspaceId = project.projectId;
            m_state.workspace.displayName = project.displayName;
            m_state.workspace.root = project.canonicalRoot;
            m_state.workspace.sourceKind = project.legacyCompatibility
                                               ? SagaLauncherWorkspaceSourceKind::LegacyWorkspace
                                               : SagaLauncherWorkspaceSourceKind::ProjectOverlay;
            m_state.workspace.status = SagaLauncherActionStatus::Passed;
        }
        ResolveTargetsLocked();
        RefreshReportsLocked();
        ++m_state.revision;
    }
    EmitStateChanged();
    return command;
}

void SagaLauncherController::RefreshRecentProjects()
{
    {
        std::lock_guard lock(m_mutex);
        m_state.recentProjects = m_recentStore->Load();
        ++m_state.revision;
    }
    EmitStateChanged();
}

SagaLauncherCommandResult SagaLauncherController::ValidateProject()
{
    return RunAction(SagaLauncherActionId::ValidateProject);
}

void SagaLauncherController::ResolveTargets()
{
    {
        std::lock_guard lock(m_mutex);
        ResolveTargetsLocked();
        ++m_state.revision;
    }
    EmitStateChanged();
}

SagaLauncherCommandResult SagaLauncherController::RunAction(SagaLauncherActionId actionId)
{
    SagaLauncherCommandResult command;
    std::optional<SagaLauncherProjectSummary> project;
    {
        std::lock_guard lock(m_mutex);
        if (m_state.runningActionId.has_value())
        {
            command.status = SagaLauncherActionStatus::Blocked;
            command.diagnostics.push_back(
                Diagnostic(SagaLauncherActionDiagnostics::AlreadyRunning,
                           SagaLauncherDiagnosticSeverity::Warning,
                           "Another executable launcher action is already running.",
                           actionId));
            return command;
        }
        const auto action = std::find_if(m_state.actions.begin(),
                                         m_state.actions.end(),
                                         [actionId](const auto& candidate) {
                                             return candidate.id == actionId;
                                         });
        if (action == m_state.actions.end() || !action->canRun ||
            action->availability != SagaLauncherActionAvailability::Available)
        {
            command.status = SagaLauncherActionStatus::Unsupported;
            command.diagnostics.push_back(
                Diagnostic(SagaLauncherActionDiagnostics::NotRunnable,
                           SagaLauncherDiagnosticSeverity::Warning,
                           "The selected launcher action is disabled, hidden, or status-only.",
                           actionId));
            return command;
        }
        if (actionId == SagaLauncherActionId::RefreshReports)
        {
            RefreshReportsLocked();
            RefreshDistributionStatusLocked();
            ++m_state.revision;
            command.accepted = true;
            command.status = SagaLauncherActionStatus::Passed;
        }
        else
        {
            if (!m_state.selectedProject.has_value())
            {
                command.status = SagaLauncherActionStatus::MissingInput;
                command.diagnostics.push_back(
                    Diagnostic(SagaLauncherActionDiagnostics::NoSelectedProject,
                               SagaLauncherDiagnosticSeverity::Warning,
                               "Select a valid project before running this action.",
                               actionId));
                return command;
            }
            project = m_state.selectedProject;
            m_state.runningActionId = actionId;
            m_state.canCancelRunningAction = actionId != SagaLauncherActionId::OpenEditor;
            action->status = SagaLauncherActionStatus::Running;
            action->canCancel = m_state.canCancelRunningAction;
            ++m_state.revision;
            command.accepted = true;
            command.status = SagaLauncherActionStatus::Running;
        }
    }
    EmitStateChanged();
    if (!project.has_value())
        return command;

    const bool started = m_taskRunner->Start(
        [this, actionId, project = *project](std::stop_token stopToken) {
            CompleteAction(m_executor->Execute(actionId, project, stopToken));
        });
    if (!started)
    {
        SagaLauncherActionResult result;
        result.actionId = actionId;
        result.status = SagaLauncherActionStatus::Blocked;
        result.diagnostics.push_back(Diagnostic(SagaLauncherActionDiagnostics::AlreadyRunning,
                                                SagaLauncherDiagnosticSeverity::Error,
                                                "Launcher task runner rejected a second action.",
                                                actionId));
        command.diagnostics = result.diagnostics;
        CompleteAction(std::move(result));
        command.accepted = false;
        command.status = SagaLauncherActionStatus::Blocked;
    }
    return command;
}

SagaLauncherCommandResult SagaLauncherController::CancelAction(SagaLauncherActionId action)
{
    SagaLauncherCommandResult command;
    {
        std::lock_guard lock(m_mutex);
        if (!m_state.runningActionId.has_value() || *m_state.runningActionId != action ||
            !m_state.canCancelRunningAction)
        {
            command.status = SagaLauncherActionStatus::Blocked;
            command.diagnostics.push_back(
                Diagnostic(SagaLauncherActionDiagnostics::NotRunnable,
                           SagaLauncherDiagnosticSeverity::Warning,
                           "The action is not the active cancellable launcher action.",
                           action));
            return command;
        }
    }
    command.accepted = m_taskRunner->RequestStop();
    command.status = command.accepted ? SagaLauncherActionStatus::Running
                                      : SagaLauncherActionStatus::Blocked;
    return command;
}

void SagaLauncherController::RefreshReports()
{
    {
        std::lock_guard lock(m_mutex);
        RefreshReportsLocked();
        ++m_state.revision;
    }
    EmitStateChanged();
}

void SagaLauncherController::RefreshDistributionStatus()
{
    {
        std::lock_guard lock(m_mutex);
        RefreshDistributionStatusLocked();
        ++m_state.revision;
    }
    EmitStateChanged();
}

SagaLauncherState SagaLauncherController::GetState() const
{
    std::lock_guard lock(m_mutex);
    return m_state;
}

void SagaLauncherController::SetStateChangedCallback(StateChangedCallback callback)
{
    std::lock_guard lock(m_mutex);
    m_callback = std::move(callback);
}

void SagaLauncherController::ResolveTargetsLocked()
{
    m_state.targets = m_targetResolver->ResolveTargets(m_state.selectedProject);
    m_state.actions = m_targetResolver->ResolveActions(m_state.selectedProject);
}

void SagaLauncherController::RefreshReportsLocked()
{
    m_state.reports = m_reportCatalog->Read(m_state.selectedProject);
}

void SagaLauncherController::RefreshDistributionStatusLocked()
{
    m_state.distribution = m_distributionReader->Read();
}

void SagaLauncherController::CompleteAction(SagaLauncherActionResult result)
{
    {
        std::lock_guard lock(m_mutex);
        const auto action = std::find_if(m_state.actions.begin(),
                                         m_state.actions.end(),
                                         [&result](const auto& candidate) {
                                             return candidate.id == result.actionId;
                                         });
        if (action != m_state.actions.end())
        {
            action->status = result.status;
            action->canCancel = false;
            action->diagnostics.insert(action->diagnostics.end(),
                                       result.diagnostics.begin(),
                                       result.diagnostics.end());
        }
        m_state.runningActionId.reset();
        m_state.canCancelRunningAction = false;
        m_state.diagnostics.insert(m_state.diagnostics.end(),
                                   result.diagnostics.begin(),
                                   result.diagnostics.end());
        RefreshReportsLocked();
        m_state.reports.insert(m_state.reports.end(),
                               result.reportReferences.begin(),
                               result.reportReferences.end());
        RefreshDistributionStatusLocked();
        ++m_state.revision;
    }
    EmitStateChanged();
}

void SagaLauncherController::EmitStateChanged()
{
    StateChangedCallback callback;
    SagaLauncherState snapshot;
    {
        std::lock_guard lock(m_mutex);
        callback = m_callback;
        snapshot = m_state;
    }
    if (callback)
        callback(snapshot);
}

} // namespace SagaProduct
