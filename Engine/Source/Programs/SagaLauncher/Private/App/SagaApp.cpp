// SPDX-License-Identifier: MPL-2.0
/// @file SagaApp.cpp
/// @brief Saga product orchestration lifecycle implementation.

#include "App/SagaApp.h"

#include "ProductIntegration/SagaLocalCollaborationMetadataReports.h"
#include "ProductIntegration/SagaLocalWorkspaceTransactionReport.h"
#include "Launcher/SagaLauncherWindow.h"
#include "Reports/SagaProductWorkflowSmokeReport.h"
#include "Workflows/SagaScriptGate.h"

#include <QApplication>
#include <QByteArray>

#include <filesystem>
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
    return config.localWorkspaceApprovalGateSmoke ||
        config.localWorkspaceSliceSmoke ||
        config.localWorkspaceRoleSmoke ||
        config.localWorkspaceReviewSmoke ||
        config.localWorkspacePresenceLockSmoke ||
        config.localWorkspaceTransactionSmoke ||
        config.workflowSmoke ||
        config.validateSagaScript ||
        config.prepareOnly ||
        config.target == SagaProductTargetKind::Runtime ||
        config.target == SagaProductTargetKind::Server;
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
    output << "diagnostic.stage=" << ToString(diagnostic.stage) << '\n';
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
    if (config.validateSagaScript)
    {
        SagaScriptGateRequest request;
        request.projectManifest = config.sagaScriptProjectManifest;
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
