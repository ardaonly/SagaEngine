/// @file SagaLauncherModel.h
/// @brief Qt-independent Product Launcher state and action vocabulary.

#pragma once

#include "Processes/SagaProcessService.h"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace SagaProduct
{

enum class SagaLauncherActionId
{
    OpenProject,
    ValidateProject,
    OpenEditor,
    RuntimeStarterArenaSmoke,
    RuntimeStarterArenaPlayable,
    FirstPlayableCheck,
    RefreshReports,
    ViewSagaScriptEvidence,
    ViewVisualBlocksEvidence,
    ViewPackageStatus,
    ViewLocalWorkspaceEvidence,
    UnsupportedGenericRuntime,
    UnsupportedServer,
    UnsupportedWorldServer,
    UnsupportedCloudCollaboration,
};

enum class SagaLauncherActionAvailability
{
    Available,
    Disabled,
    Hidden
};

enum class SagaLauncherActionStatus
{
    Available,
    Disabled,
    Hidden,
    Running,
    Passed,
    PassedWithLimitations,
    Failed,
    Blocked,
    Unsupported,
    NotConfigured,
    MissingInput,
    Unknown,
    Cancelled,
};

enum class SagaLauncherDiagnosticSeverity
{
    Info,
    Warning,
    Error
};

enum class SagaLauncherWorkspaceSourceKind
{
    Builtin,
    ProjectOverlay,
};

enum class SagaLauncherTargetKind
{
    EditorTarget,
    RuntimeStarterArenaSmokeTarget,
    RuntimeStarterArenaPlayableTarget,
    FirstPlayableCheckTarget,
    SagascriptEvidenceTarget,
    VisualBlocksEvidenceTarget,
    PackageStatusTarget,
    UnsupportedGenericRuntimeTarget,
    UnsupportedServerTarget,
    UnsupportedWorldServerTarget,
    UnsupportedCloudCollaborationTarget,
};

struct SagaLauncherDiagnostic
{
    std::string diagnosticId;
    SagaLauncherDiagnosticSeverity severity = SagaLauncherDiagnosticSeverity::Info;
    std::string message;
    std::optional<std::filesystem::path> path;
    std::optional<SagaLauncherActionId> actionId;
    std::optional<std::filesystem::path> reportPath;
    std::optional<std::string> nonClaim;
};

struct SagaLauncherReportReference
{
    std::string reportType;
    std::string owner;
    std::filesystem::path path;
    bool exists = false;
    int schemaVersion = -1;
    std::string rawStatus;
    SagaLauncherActionStatus mappedStatus = SagaLauncherActionStatus::NotConfigured;
    std::optional<bool> verified;
    std::vector<std::string> limitations;
    std::size_t diagnosticCount = 0;
    std::string lastModifiedUtc;
};

struct SagaLauncherProjectSummary
{
    std::string projectId;
    std::string displayName;
    std::filesystem::path canonicalRoot;
    std::filesystem::path canonicalManifestPath;
    int schemaVersion = -1;
    bool valid = false;
    std::vector<SagaLauncherDiagnostic> diagnostics;
};

struct SagaLauncherRecentProject
{
    std::string projectId;
    std::string displayName;
    std::filesystem::path canonicalManifestPath;
    std::filesystem::path canonicalRoot;
    std::string lastOpenedUtc;
    bool exists = false;
    std::vector<SagaLauncherDiagnostic> diagnostics;
};

struct SagaLauncherWorkspaceSummary
{
    std::string workspaceId;
    std::string displayName;
    std::filesystem::path root;
    SagaLauncherWorkspaceSourceKind sourceKind = SagaLauncherWorkspaceSourceKind::Builtin;
    std::string editorProfile;
    std::string runtimeRole;
    std::string serverRole;
    SagaLauncherActionStatus status = SagaLauncherActionStatus::NotConfigured;
    std::vector<SagaLauncherDiagnostic> diagnostics;
};

struct SagaLauncherTargetSummary
{
    SagaLauncherTargetKind targetKind = SagaLauncherTargetKind::EditorTarget;
    std::string displayName;
    SagaLauncherActionAvailability availability = SagaLauncherActionAvailability::Disabled;
    SagaLauncherActionStatus status = SagaLauncherActionStatus::Disabled;
    std::string owner;
    std::vector<std::string> requiredInputs;
    std::vector<SagaLauncherDiagnostic> diagnostics;
    std::vector<std::string> nonClaims;
};

struct SagaLauncherDistributionSummary
{
    std::filesystem::path sourcePath;
    std::string sourceKind;
    std::string packageName;
    std::string version;
    std::string gitCommit;
    std::string platform;
    std::string rawStatus;
    SagaLauncherActionStatus mappedStatus = SagaLauncherActionStatus::NotConfigured;
    bool verified = false;
    std::vector<std::string> includedApplications;
    std::vector<std::string> includedPublicTools;
    std::vector<std::string> excludedRetiredTools;
    std::vector<std::string> excludedDevTools;
    std::vector<std::string> limitations;
    std::vector<SagaLauncherDiagnostic> diagnostics;
    std::filesystem::path archivePath;
    std::filesystem::path checksumPath;
};

struct SagaLauncherAction
{
    SagaLauncherActionId id = SagaLauncherActionId::OpenProject;
    std::string label;
    std::string description;
    SagaLauncherActionAvailability availability = SagaLauncherActionAvailability::Disabled;
    SagaLauncherActionStatus status = SagaLauncherActionStatus::Disabled;
    std::string owner;
    std::optional<SagaProcessTargetId> processTargetId;
    std::vector<std::string> requiredInputKinds;
    std::vector<SagaLauncherReportReference> reportReferences;
    std::vector<SagaLauncherDiagnostic> diagnostics;
    std::vector<std::string> nonClaims;
    bool canRun = false;
    bool canCancel = false;
};

struct SagaLauncherActionResult
{
    SagaLauncherActionId actionId = SagaLauncherActionId::OpenProject;
    SagaLauncherActionStatus status = SagaLauncherActionStatus::Unknown;
    bool started = false;
    bool cancelled = false;
    std::optional<int> exitCode;
    SagaProcessExitClassification processExitClassification =
        SagaProcessExitClassification::NotStarted;
    std::chrono::milliseconds duration{0};
    std::vector<SagaLauncherReportReference> reportReferences;
    std::vector<SagaLauncherDiagnostic> diagnostics;
    std::string standardOutputEvaluation;
    std::string standardErrorEvaluation;
};

struct SagaLauncherState
{
    std::uint64_t revision = 0;
    std::optional<SagaLauncherProjectSummary> selectedProject;
    std::vector<SagaLauncherRecentProject> recentProjects;
    SagaLauncherWorkspaceSummary workspace;
    std::vector<SagaLauncherTargetSummary> targets;
    std::vector<SagaLauncherAction> actions;
    std::vector<SagaLauncherReportReference> reports;
    SagaLauncherDistributionSummary distribution;
    std::vector<SagaLauncherDiagnostic> diagnostics;
    std::optional<SagaLauncherActionId> runningActionId;
    bool canCancelRunningAction = false;
};

[[nodiscard]] const char* ToString(SagaLauncherActionId id) noexcept;
[[nodiscard]] const char* ToString(SagaLauncherActionAvailability availability) noexcept;
[[nodiscard]] const char* ToString(SagaLauncherActionStatus status) noexcept;
[[nodiscard]] const char* ToString(SagaLauncherDiagnosticSeverity severity) noexcept;
[[nodiscard]] const char* ToString(SagaLauncherTargetKind kind) noexcept;
[[nodiscard]] std::optional<SagaLauncherActionId> ParseLauncherActionId(
    const std::string& token) noexcept;

} // namespace SagaProduct
