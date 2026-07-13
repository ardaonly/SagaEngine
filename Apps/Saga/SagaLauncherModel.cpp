/// @file SagaLauncherModel.cpp

#include "SagaLauncherModel.h"

namespace SagaProduct
{

const char* ToString(SagaLauncherActionId id) noexcept
{
    switch (id)
    {
    case SagaLauncherActionId::OpenProject:
        return "open-project";
    case SagaLauncherActionId::ValidateProject:
        return "validate-project";
    case SagaLauncherActionId::OpenEditor:
        return "open-editor";
    case SagaLauncherActionId::RuntimeStarterArenaSmoke:
        return "runtime-starter-arena-smoke";
    case SagaLauncherActionId::RuntimeStarterArenaPlayable:
        return "runtime-starter-arena-playable";
    case SagaLauncherActionId::FirstPlayableCheck:
        return "first-playable-check";
    case SagaLauncherActionId::RefreshReports:
        return "refresh-reports";
    case SagaLauncherActionId::ViewSagaScriptEvidence:
        return "view-sagascript-evidence";
    case SagaLauncherActionId::ViewVisualBlocksEvidence:
        return "view-visual-blocks-evidence";
    case SagaLauncherActionId::ViewPackageStatus:
        return "view-package-status";
    case SagaLauncherActionId::ViewLocalWorkspaceEvidence:
        return "view-local-workspace-evidence";
    case SagaLauncherActionId::UnsupportedGenericRuntime:
        return "unsupported-generic-runtime";
    case SagaLauncherActionId::UnsupportedServer:
        return "unsupported-server";
    case SagaLauncherActionId::UnsupportedWorldServer:
        return "unsupported-world-server";
    case SagaLauncherActionId::UnsupportedCloudCollaboration:
        return "unsupported-cloud-collaboration";
    }
    return "unknown";
}

const char* ToString(SagaLauncherActionAvailability value) noexcept
{
    switch (value)
    {
    case SagaLauncherActionAvailability::Available:
        return "available";
    case SagaLauncherActionAvailability::Disabled:
        return "disabled";
    case SagaLauncherActionAvailability::Hidden:
        return "hidden";
    }
    return "disabled";
}

const char* ToString(SagaLauncherActionStatus value) noexcept
{
    switch (value)
    {
    case SagaLauncherActionStatus::Available:
        return "available";
    case SagaLauncherActionStatus::Disabled:
        return "disabled";
    case SagaLauncherActionStatus::Hidden:
        return "hidden";
    case SagaLauncherActionStatus::Running:
        return "running";
    case SagaLauncherActionStatus::Passed:
        return "passed";
    case SagaLauncherActionStatus::PassedWithLimitations:
        return "passed-with-limitations";
    case SagaLauncherActionStatus::Failed:
        return "failed";
    case SagaLauncherActionStatus::Blocked:
        return "blocked";
    case SagaLauncherActionStatus::Unsupported:
        return "unsupported";
    case SagaLauncherActionStatus::NotConfigured:
        return "not-configured";
    case SagaLauncherActionStatus::MissingInput:
        return "missing-input";
    case SagaLauncherActionStatus::Unknown:
        return "unknown";
    case SagaLauncherActionStatus::Cancelled:
        return "cancelled";
    }
    return "unknown";
}

const char* ToString(SagaLauncherDiagnosticSeverity value) noexcept
{
    switch (value)
    {
    case SagaLauncherDiagnosticSeverity::Info:
        return "info";
    case SagaLauncherDiagnosticSeverity::Warning:
        return "warning";
    case SagaLauncherDiagnosticSeverity::Error:
        return "error";
    }
    return "info";
}

const char* ToString(SagaLauncherTargetKind value) noexcept
{
    switch (value)
    {
    case SagaLauncherTargetKind::EditorTarget:
        return "editor";
    case SagaLauncherTargetKind::RuntimeStarterArenaSmokeTarget:
        return "runtime-starter-arena-smoke";
    case SagaLauncherTargetKind::RuntimeStarterArenaPlayableTarget:
        return "runtime-starter-arena-playable";
    case SagaLauncherTargetKind::FirstPlayableCheckTarget:
        return "first-playable-check";
    case SagaLauncherTargetKind::SagascriptEvidenceTarget:
        return "sagascript-evidence";
    case SagaLauncherTargetKind::VisualBlocksEvidenceTarget:
        return "visual-blocks-evidence";
    case SagaLauncherTargetKind::PackageStatusTarget:
        return "package-status";
    case SagaLauncherTargetKind::UnsupportedGenericRuntimeTarget:
        return "generic-runtime";
    case SagaLauncherTargetKind::UnsupportedServerTarget:
        return "server";
    case SagaLauncherTargetKind::UnsupportedWorldServerTarget:
        return "world-server";
    case SagaLauncherTargetKind::UnsupportedCloudCollaborationTarget:
        return "cloud-collaboration";
    }
    return "unknown";
}

std::optional<SagaLauncherActionId> ParseLauncherActionId(const std::string& token) noexcept
{
    for (const auto id : {SagaLauncherActionId::OpenProject,
                          SagaLauncherActionId::ValidateProject,
                          SagaLauncherActionId::OpenEditor,
                          SagaLauncherActionId::RuntimeStarterArenaSmoke,
                          SagaLauncherActionId::RuntimeStarterArenaPlayable,
                          SagaLauncherActionId::FirstPlayableCheck,
                          SagaLauncherActionId::RefreshReports,
                          SagaLauncherActionId::ViewSagaScriptEvidence,
                          SagaLauncherActionId::ViewVisualBlocksEvidence,
                          SagaLauncherActionId::ViewPackageStatus,
                          SagaLauncherActionId::ViewLocalWorkspaceEvidence,
                          SagaLauncherActionId::UnsupportedGenericRuntime,
                          SagaLauncherActionId::UnsupportedServer,
                          SagaLauncherActionId::UnsupportedWorldServer,
                          SagaLauncherActionId::UnsupportedCloudCollaboration})
    {
        if (token == ToString(id))
            return id;
    }
    return std::nullopt;
}

} // namespace SagaProduct
