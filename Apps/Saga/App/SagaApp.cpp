/// @file SagaApp.cpp
/// @brief Saga product orchestration lifecycle implementation.

#include "App/SagaApp.h"

#include "FirstPlayableHumanCapture.h"
#include "FirstPlayableWorkflow.h"
#include "SagaLocalCollaborationMetadataReports.h"
#include "SagaLocalWorkspaceTransactionReport.h"
#include "Launcher/SagaLauncherWindow.h"
#include "SagaPackageStaging.h"
#include "SagaProjectSystem.h"
#include "SagaProductWorkflowSmokeReport.h"
#include "SagaPublishReadiness.h"
#include "SagaScriptGate.h"

#include <QApplication>
#include <QByteArray>

#include <filesystem>
#include <chrono>
#include <cstdlib>
#include <memory>
#include <optional>
#include <string>
#include <utility>

namespace SagaProduct
{
namespace
{

constexpr int kExitOk = 0;
constexpr int kExitStartupFailure = 1;
constexpr int kExitConfigurationFailure = 2;

[[nodiscard]] std::filesystem::path ResolvePreparedExecutablePath(
    const SagaAppConfig& config,
    const SagaPreparedTarget& prepared)
{
    std::filesystem::path executablePath = prepared.executableName;
    if (executablePath.is_relative() && !config.executablePath.empty())
    {
        executablePath = config.executablePath.parent_path() / executablePath;
    }
#if defined(_WIN32)
    if (executablePath.extension().empty() &&
        !std::filesystem::exists(executablePath))
    {
        executablePath += ".exe";
    }
#endif
    return executablePath;
}

[[nodiscard]] SagaProcessLaunchRequest MakeLaunchRequest(
    const SagaAppConfig& config,
    const SagaPreparedTarget& prepared)
{
    SagaProcessLaunchRequest request;
    request.target = prepared.kind;
    request.executablePath = ResolvePreparedExecutablePath(config, prepared);
    request.arguments = prepared.arguments;
    request.workingDirectory = request.executablePath.parent_path();
    return request;
}

[[nodiscard]] bool ShouldUsePreparationFlow(
    const SagaAppConfig& config) noexcept
{
    return config.publishCheck ||
        config.firstPlayableHumanCapture ||
        config.firstPlayableCheck ||
        config.localWorkspaceApprovalGateSmoke ||
        config.localWorkspaceSliceSmoke ||
        config.localWorkspaceRoleSmoke ||
        config.localWorkspaceReviewSmoke ||
        config.localWorkspacePresenceLockSmoke ||
        config.localWorkspaceTransactionSmoke ||
        config.workflowSmoke ||
        config.stagePackages ||
        config.validateSagaScript ||
        config.prepareOnly ||
        config.target == SagaProductTargetKind::Runtime ||
        config.target == SagaProductTargetKind::Server;
}

[[nodiscard]] FirstPlayableWorkflowRequest MakeFirstPlayableRequest(
    const SagaAppConfig& config, bool humanCapture)
{
    FirstPlayableWorkflowRequest request;
    request.runtime.projectManifest = config.workflowProjectPath;
    request.runtime.runtimeExecutable = config.runtimeExecutable.empty() ?
        config.executablePath.parent_path() / "SagaRuntime" :
        config.runtimeExecutable;
    request.runtime.sagaScriptExecutable = config.sagaScriptExecutable;
    request.runtime.runtimeBridgeAssembly = config.runtimeBridgeAssembly;
    if (request.runtime.runtimeBridgeAssembly.empty())
    {
        if (const char* bridge = std::getenv("SAGASCRIPT_RUNTIME_BRIDGE_ASSEMBLY"))
            request.runtime.runtimeBridgeAssembly = bridge;
    }
    if (request.runtime.runtimeBridgeAssembly.empty())
    {
        request.runtime.runtimeBridgeAssembly =
            config.executablePath.parent_path().parent_path() /
            "Managed" / "SagaScript.RuntimeBridge" /
            "SagaScript.RuntimeBridge.dll";
    }
    request.runtime.timeout = std::chrono::milliseconds(config.firstPlayableTimeoutMs);
    if (config.firstPlayableOutputDirectory.empty())
    {
        const auto stamp = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        request.runtime.outputDirectory = std::filesystem::temp_directory_path() /
            ((humanCapture ? "saga-first-playable-human-" : "saga-first-playable-") +
             std::to_string(stamp));
    }
    else request.runtime.outputDirectory = config.firstPlayableOutputDirectory;
    request.summaryPath = config.firstPlayableSummaryPath.empty() ?
        request.runtime.outputDirectory / "first_playable_summary.json" :
        config.firstPlayableSummaryPath;
    if (!config.firstPlayableKeyboardReportPath.empty())
        request.keyboardReportPath = config.firstPlayableKeyboardReportPath;
    return request;
}

void ConfigureBundledRuntimeEnvironment(const SagaAppConfig& config)
{
#if defined(Q_OS_LINUX)
    if (qgetenv("QT_QPA_PLATFORM").isEmpty())
    {
        qputenv("QT_QPA_PLATFORM", "xcb");
    }
#endif

    if (!qgetenv("FONTCONFIG_PATH").isEmpty())
    {
        return;
    }

    const std::filesystem::path fontconfigRoot =
        config.executablePath.parent_path().parent_path() /
        "config" / "fontconfig";
    if (std::filesystem::exists(fontconfigRoot / "fonts.conf"))
    {
        qputenv("FONTCONFIG_PATH",
            QByteArray(fontconfigRoot.string().c_str()));
    }
}

void WriteProductDiagnostic(std::ostream& output,
                            const SagaProductDiagnostic& diagnostic)
{
    output << "diagnostic.id=" << diagnostic.diagnosticId << '\n';
    output << "diagnostic.target=" << ToString(diagnostic.target) << '\n';
    output << "diagnostic.phase=" << ToString(diagnostic.phase) << '\n';
    output << "diagnostic.message=" << diagnostic.message << '\n';
    if (diagnostic.path.has_value())
    {
        output << "diagnostic.path=" << diagnostic.path->string() << '\n';
    }
}

} // namespace

SagaApp::SagaApp()
    : SagaApp(std::make_unique<SagaProcessLauncher>())
{
}

SagaApp::SagaApp(std::unique_ptr<ISagaProcessLauncher> processLauncher)
    : m_processLauncher(processLauncher ?
          std::move(processLauncher) : std::make_unique<SagaProcessLauncher>())
{
}

int SagaApp::Run(int argc,
                 char* argv[],
                 const SagaAppConfig& config,
                 std::ostream& out,
                 std::ostream& err)
{
    if (ShouldUsePreparationFlow(config))
    {
        return Run(config, out, err);
    }

    ConfigureBundledRuntimeEnvironment(config);

    QApplication qtApp(argc, argv);
    SagaLauncherWindow window(
        config.executablePath, config.launcherDistributionReportPath);
    window.show();
    return qtApp.exec();
}

int SagaApp::Run(const SagaAppConfig& config,
                 std::ostream& out,
                 std::ostream& err)
{
    if (config.firstPlayableHumanCapture)
    {
        if (config.workflowProjectPath.empty())
        {
            err << "Saga: --first-playable-human-capture requires --project <.sagaproj>\n";
            return kExitConfigurationFailure;
        }
        FirstPlayableHumanCaptureRequest request;
        request.workflow = MakeFirstPlayableRequest(config, true);
        request.frames = static_cast<std::uint32_t>(config.firstPlayableHumanFrames);
        request.timeout = std::chrono::milliseconds(
            config.firstPlayableHumanTimeoutMs);
        FirstPlayableHumanCapture capture;
        const auto result = capture.Run(request, out, err);
        if (result.status == EvidenceStatus::Passed) return kExitOk;
        return result.status == EvidenceStatus::Incomplete ?
            kExitConfigurationFailure : kExitStartupFailure;
    }

    if (config.firstPlayableCheck)
    {
        if (config.workflowProjectPath.empty())
        {
            err << "Saga: --first-playable-check requires --project <.sagaproj>\n";
            return kExitConfigurationFailure;
        }

        FirstPlayableWorkflowRequest request = MakeFirstPlayableRequest(config, false);

        const FirstPlayableWorkflowResult result =
            RunFirstPlayableWorkflow(request, out, err);
        if (result.gate.status == FirstPlayableGateStatus::Accepted ||
            result.gate.status == FirstPlayableGateStatus::AcceptedWithManualEvidencePending)
            return kExitOk;
        return result.gate.status == FirstPlayableGateStatus::Incomplete ?
            kExitConfigurationFailure : kExitStartupFailure;
    }

    if (config.validateSagaScript)
    {
        SagaScriptGateRequest request;
        request.projectRoot = config.sagaScriptProjectRoot;
        request.forgeExecutable = config.forgeExecutable;
        request.sagaScriptExecutable = config.sagaScriptExecutable;

        SagaScriptGate gate;
        const SagaScriptGateResult result =
            gate.ValidateProject(request, out, err);
        for (const SagaProductDiagnostic& diagnostic : result.diagnostics)
        {
            WriteProductDiagnostic(err, diagnostic);
        }

        out << "sagascript.source=" << result.paths.sourceRoot.string() << '\n';
        out << "sagascript.manifests="
            << result.paths.manifestOutputDirectory.string() << '\n';
        out << "sagascript.diagnostics="
            << result.paths.diagnosticsOutputPath.string() << '\n';
        if (result.started)
        {
            out << "sagascript.exitCode=" << result.exitCode << '\n';
        }

        if (result.ok)
        {
            return kExitOk;
        }
        return result.started && result.exitCode != 0 ?
            result.exitCode : kExitStartupFailure;
    }

    if (config.workflowSmoke)
    {
        if (config.workflowProjectPath.empty())
        {
            err << "Saga: --workflow-smoke requires --project <.sagaproj>\n";
            return kExitStartupFailure;
        }
        if (config.workflowReportPath.empty())
        {
            err << "Saga: --workflow-smoke requires --workflow-report-out <path>\n";
            return kExitStartupFailure;
        }

        SagaProductWorkflowSmokeRequest request;
        request.projectManifestPath = config.workflowProjectPath;
        request.profile = config.workflowProfile;
        request.reportPath = config.workflowReportPath;

        const SagaProductWorkflowSmokeResult result =
            WriteProductWorkflowSmokeReport(request);
        if (!result.ok)
        {
            err << result.error << '\n';
            return kExitStartupFailure;
        }

        out << "workflow.report=" << result.reportPath.string() << '\n';
        out << "workflow.status=" << result.status << '\n';
        return kExitOk;
    }

    if (config.localWorkspaceTransactionSmoke)
    {
        if (config.workflowProjectPath.empty())
        {
            err << "Saga: --local-workspace-transaction-smoke requires "
                << "--project <.sagaproj>\n";
            return kExitStartupFailure;
        }
        if (config.localWorkspaceTransactionReportPath.empty())
        {
            err << "Saga: --local-workspace-transaction-smoke requires "
                << "--transaction-report-out <path>\n";
            return kExitStartupFailure;
        }

        SagaLocalWorkspaceTransactionRequest request;
        request.projectManifestPath = config.workflowProjectPath;
        request.workspaceSelector = config.workspaceSelector;
        request.actorId = config.localWorkspaceActorId;
        request.operationKind = config.localWorkspaceOperationKind;
        request.reportPath = config.localWorkspaceTransactionReportPath;

        const SagaLocalWorkspaceTransactionResult result =
            WriteLocalWorkspaceTransactionReport(request);
        if (!result.ok)
        {
            err << result.error << '\n';
            return kExitStartupFailure;
        }

        out << "transaction.report=" << result.reportPath.string() << '\n';
        out << "transaction.status=" << result.status << '\n';
        return kExitOk;
    }

    if (config.localWorkspacePresenceLockSmoke)
    {
        if (config.workflowProjectPath.empty())
        {
            err << "Saga: --local-workspace-presence-lock-smoke requires "
                << "--project <.sagaproj>\n";
            return kExitStartupFailure;
        }
        if (config.localWorkspaceLockTargetPath.empty())
        {
            err << "Saga: --local-workspace-presence-lock-smoke requires "
                << "--lock-target <path>\n";
            return kExitStartupFailure;
        }
        if (config.localWorkspacePresenceLockReportPath.empty())
        {
            err << "Saga: --local-workspace-presence-lock-smoke requires "
                << "--presence-lock-report-out <path>\n";
            return kExitStartupFailure;
        }

        SagaLocalPresenceLockReportRequest request;
        request.projectManifestPath = config.workflowProjectPath;
        request.workspaceSelector = config.workspaceSelector;
        request.actorId = config.localWorkspaceActorId;
        request.lockTargetPath = config.localWorkspaceLockTargetPath;
        request.reportPath = config.localWorkspacePresenceLockReportPath;

        const SagaLocalCollaborationMetadataReportResult result =
            WriteLocalPresenceLockReport(request);
        if (!result.ok)
        {
            err << result.error << '\n';
            return kExitStartupFailure;
        }

        out << "presence_lock.report=" << result.reportPath.string() << '\n';
        out << "presence_lock.status=" << result.status << '\n';
        return kExitOk;
    }

    if (config.localWorkspaceReviewSmoke)
    {
        if (config.workflowProjectPath.empty())
        {
            err << "Saga: --local-workspace-review-smoke requires "
                << "--project <.sagaproj>\n";
            return kExitStartupFailure;
        }
        if (config.localWorkspaceReviewTargetPath.empty())
        {
            err << "Saga: --local-workspace-review-smoke requires "
                << "--review-target <path>\n";
            return kExitStartupFailure;
        }
        if (config.localWorkspaceReviewReportPath.empty())
        {
            err << "Saga: --local-workspace-review-smoke requires "
                << "--review-report-out <path>\n";
            return kExitStartupFailure;
        }

        SagaLocalReviewAuditReportRequest request;
        request.projectManifestPath = config.workflowProjectPath;
        request.workspaceSelector = config.workspaceSelector;
        request.actorId = config.localWorkspaceActorId;
        request.reviewTargetPath = config.localWorkspaceReviewTargetPath;
        request.comment = config.localWorkspaceReviewComment;
        request.reportPath = config.localWorkspaceReviewReportPath;

        const SagaLocalCollaborationMetadataReportResult result =
            WriteLocalReviewAuditReport(request);
        if (!result.ok)
        {
            err << result.error << '\n';
            return kExitStartupFailure;
        }

        out << "review.report=" << result.reportPath.string() << '\n';
        out << "review.status=" << result.status << '\n';
        return kExitOk;
    }

    if (config.localWorkspaceRoleSmoke)
    {
        if (config.workflowProjectPath.empty())
        {
            err << "Saga: --local-workspace-role-smoke requires "
                << "--project <.sagaproj>\n";
            return kExitStartupFailure;
        }
        if (config.localWorkspaceRoleName.empty())
        {
            err << "Saga: --local-workspace-role-smoke requires "
                << "--role <name>\n";
            return kExitStartupFailure;
        }
        if (config.localWorkspacePermissionName.empty())
        {
            err << "Saga: --local-workspace-role-smoke requires "
                << "--permission <name>\n";
            return kExitStartupFailure;
        }
        if (config.localWorkspaceRoleReportPath.empty())
        {
            err << "Saga: --local-workspace-role-smoke requires "
                << "--role-report-out <path>\n";
            return kExitStartupFailure;
        }

        SagaLocalRolePermissionReportRequest request;
        request.projectManifestPath = config.workflowProjectPath;
        request.workspaceSelector = config.workspaceSelector;
        request.actorId = config.localWorkspaceActorId;
        request.roleName = config.localWorkspaceRoleName;
        request.permissionName = config.localWorkspacePermissionName;
        request.reportPath = config.localWorkspaceRoleReportPath;

        const SagaLocalCollaborationMetadataReportResult result =
            WriteLocalRolePermissionReport(request);
        if (!result.ok)
        {
            err << result.error << '\n';
            return kExitStartupFailure;
        }

        out << "role.report=" << result.reportPath.string() << '\n';
        out << "role.status=" << result.status << '\n';
        return kExitOk;
    }

    if (config.localWorkspaceSliceSmoke)
    {
        if (config.workflowProjectPath.empty())
        {
            err << "Saga: --local-workspace-slice-smoke requires "
                << "--project <.sagaproj>\n";
            return kExitStartupFailure;
        }
        if (config.localWorkspaceSliceName.empty())
        {
            err << "Saga: --local-workspace-slice-smoke requires "
                << "--slice <name>\n";
            return kExitStartupFailure;
        }
        if (config.localWorkspaceSliceTargetPath.empty())
        {
            err << "Saga: --local-workspace-slice-smoke requires "
                << "--slice-target <path>\n";
            return kExitStartupFailure;
        }
        if (config.localWorkspaceSliceReportPath.empty())
        {
            err << "Saga: --local-workspace-slice-smoke requires "
                << "--slice-report-out <path>\n";
            return kExitStartupFailure;
        }

        SagaLocalProjectSliceReportRequest request;
        request.projectManifestPath = config.workflowProjectPath;
        request.workspaceSelector = config.workspaceSelector;
        request.actorId = config.localWorkspaceActorId;
        request.sliceName = config.localWorkspaceSliceName;
        request.sliceTargetPath = config.localWorkspaceSliceTargetPath;
        request.reportPath = config.localWorkspaceSliceReportPath;

        const SagaLocalCollaborationMetadataReportResult result =
            WriteLocalProjectSliceReport(request);
        if (!result.ok)
        {
            err << result.error << '\n';
            return kExitStartupFailure;
        }

        out << "slice.report=" << result.reportPath.string() << '\n';
        out << "slice.status=" << result.status << '\n';
        return kExitOk;
    }

    if (config.localWorkspaceApprovalGateSmoke)
    {
        if (config.workflowProjectPath.empty())
        {
            err << "Saga: --local-workspace-approval-gate-smoke requires "
                << "--project <.sagaproj>\n";
            return kExitStartupFailure;
        }
        if (config.localWorkspaceRoleName.empty())
        {
            err << "Saga: --local-workspace-approval-gate-smoke requires "
                << "--role <name>\n";
            return kExitStartupFailure;
        }
        if (config.localWorkspaceGateTargetPath.empty())
        {
            err << "Saga: --local-workspace-approval-gate-smoke requires "
                << "--gate-target <path>\n";
            return kExitStartupFailure;
        }
        if (config.localWorkspaceApprovalGateReportPath.empty())
        {
            err << "Saga: --local-workspace-approval-gate-smoke requires "
                << "--approval-gate-report-out <path>\n";
            return kExitStartupFailure;
        }

        SagaLocalApprovalGateReportRequest request;
        request.projectManifestPath = config.workflowProjectPath;
        request.workspaceSelector = config.workspaceSelector;
        request.actorId = config.localWorkspaceActorId;
        request.roleName = config.localWorkspaceRoleName;
        request.gateTargetPath = config.localWorkspaceGateTargetPath;
        request.approvalState = config.localWorkspaceApprovalState;
        request.reportPath = config.localWorkspaceApprovalGateReportPath;

        const SagaLocalCollaborationMetadataReportResult result =
            WriteLocalApprovalGateReport(request);
        if (!result.ok)
        {
            err << result.error << '\n';
            return kExitStartupFailure;
        }

        out << "approval_gate.report=" << result.reportPath.string() << '\n';
        out << "approval_gate.status=" << result.status << '\n';
        return kExitOk;
    }

    if (config.stagePackages)
    {
        SagaPackageStagingRequest request;
        request.projectRoot = config.packageStageProjectRoot;
        request.profile = config.packageProfile;
        request.targetPlatform = config.targetPlatform;
        request.runtimeCompatibilityVersion =
            config.runtimeCompatibilityVersion;
        request.reportPath = config.packageStageReportPath;

        SagaPackageStagingService service;
        const SagaPackageStagingResult result = service.Stage(request);

        out << "package.report=" << result.paths.reportPath.string() << '\n';
        out << "package.clientManifest="
            << result.paths.clientPackageManifest.string() << '\n';
        out << "package.serverManifest="
            << result.paths.serverPackageManifest.string() << '\n';
        out << "package.status=" << (result.ok ? "staged" : "blocked")
            << '\n';
        for (const SagaProductDiagnostic& diagnostic : result.diagnostics)
        {
            WriteProductDiagnostic(err, diagnostic);
        }

        return result.ok ? kExitOk : kExitStartupFailure;
    }

    if (config.publishCheck)
    {
        std::string diagnosticsError;
        SagaPublishReadinessRequest request;
        request.projectRoot = config.publishProjectRoot;
        request.profile = config.publishProfile;
        request.reportPath = config.publishReportPath;
        request.diagnosticsInputs =
            SagaPublishReadinessService::ParseDiagnosticsInputs(
                config.publishDiagnostics,
                diagnosticsError);
        if (!diagnosticsError.empty())
        {
            err << diagnosticsError << '\n';
            return kExitStartupFailure;
        }

        SagaPublishReadinessService service;
        const SagaPublishReadinessResult result = service.Check(request);

        out << "publish.report=" << result.reportPath.string() << '\n';
        out << "publish.status="
            << ToString(result.report.readiness.status) << '\n';
        out << "publish.blockers="
            << result.report.readiness.blockers.size() << '\n';
        for (const auto& blocker : result.report.readiness.blockers)
        {
            err << "publish.blocker.kind=" << ToString(blocker.kind) << '\n';
            err << "publish.blocker.message=" << blocker.message << '\n';
        }

        return result.ok ? kExitOk : kExitStartupFailure;
    }

    std::string productInfoError;
    const SagaProductInfo productInfo =
        LoadProductInfo(config, productInfoError);
    if (!productInfoError.empty())
    {
        err << productInfoError << '\n';
    }

    SagaWorkspaceResolveRequest request;
    request.selector = config.workspaceSelector;
    request.builtInBasicRoot = config.builtInWorkspaceRoot;

    const SagaWorkspaceResolveResult workspaceResult =
        m_workspaceResolver.Resolve(request);
    if (!workspaceResult.ok)
    {
        err << workspaceResult.error << '\n';
        for (const std::string& diagnostic : workspaceResult.diagnostics)
        {
            err << diagnostic << '\n';
        }
        return kExitStartupFailure;
    }

    SagaSessionModel session;
    session.workspace = workspaceResult.workspace;
    session.target = config.target;
    session.packageManifestPath = config.packageManifestPath;

    const SagaTargetPreparationResult preparation =
        m_productHost.PrepareTargetWithDiagnostics(session);
    if (!preparation.ok)
    {
        for (const SagaProductDiagnostic& diagnostic : preparation.diagnostics)
        {
            WriteProductDiagnostic(err, diagnostic);
        }
        return kExitStartupFailure;
    }

    const SagaPreparedTarget& prepared = preparation.target;
    out << productInfo.productName << " " << productInfo.buildVersion << '\n';
    out << "workspace=" << prepared.workspace.id << '\n';
    out << "target=" << ToString(prepared.kind) << '\n';
    out << "executable=" << prepared.executableName << '\n';
    out << "availability=" << ToString(prepared.availability) << '\n';
    for (const std::string& argument : prepared.arguments)
    {
        out << "argument=" << argument << '\n';
    }

    if (config.prepareOnly)
    {
        return kExitOk;
    }

    const SagaProcessLaunchRequest launchRequest =
        MakeLaunchRequest(config, prepared);
    const SagaProcessLaunchResult launchResult =
        m_processLauncher->Launch(launchRequest, out, err);
    for (const SagaProductDiagnostic& diagnostic : launchResult.diagnostics)
    {
        WriteProductDiagnostic(err, diagnostic);
    }
    if (launchResult.started)
    {
        out << "launch.exitCode=" << launchResult.exitCode << '\n';
    }

    if (!launchResult.started)
    {
        return kExitStartupFailure;
    }
    if (!launchResult.ok)
    {
        return launchResult.exitCode == 0 ?
            kExitStartupFailure : launchResult.exitCode;
    }
    return kExitOk;
}

} // namespace SagaProduct
