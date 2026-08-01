// SPDX-License-Identifier: MPL-2.0

#pragma once

#include "SagaEditor/Packages/PackageWorkflowToolAdapter.h"
#include "SagaEditor/Project/ProjectMutation.h"

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>

namespace SagaEditor
{
class EditorDocumentWorkspace;
class IEditorDiagnosticsService;
class ScriptAnalysisCoordinator;
class ScriptProjectionSession;

enum class PackageWorkflowControllerState
{
    Preparing,
    Resolving,
    Analyzing,
    AwaitingReview,
    Applying,
    Refreshing,
    Ready,
    ToolingOutOfDate,
    RecoveryRequired,
};

struct PackageWorkflowSelection final
{
    std::string packageId;
    std::string module;

    [[nodiscard]] bool Valid() const noexcept
    { return !packageId.empty() && !module.empty(); }
    bool operator==(const PackageWorkflowSelection&) const = default;
};

struct PackageRecoveryInspection final
{
    std::filesystem::path journalPath;
    int schemaVersion = 0;
    std::string transactionId;
    std::string operationId;
    std::string phase;
    bool commitMarker = false;
    std::vector<ProjectMutationFile> files;
    std::string code;
    std::string message;
};

struct PackageWorkflowControllerSnapshot final
{
    PackageWorkflowControllerState state = PackageWorkflowControllerState::Ready;
    PackageWorkflowSelection selection;
    std::optional<PackageTransactionReview> review;
    std::vector<PackageRecoveryInspection> recovery;
    std::string progress;
    std::string code;
    std::string message;
    bool active = false;
    bool canApply = false;
    bool canCancel = false;
    bool canRetry = false;
    bool canUndo = false;
    bool canRedo = false;
};

/// Host-owned package orchestration authority. It serializes semantic intent,
/// transaction publication, derived refresh, diagnostics, history, and
/// recovery while exposing immutable snapshots to UI consumers.
class PackageWorkflowController final
{
public:
    using ChangeSubscription = std::uint64_t;
    using ChangeCallback = std::function<void(const PackageWorkflowControllerSnapshot&)>;
    static constexpr ChangeSubscription InvalidChangeSubscription = 0;

    PackageWorkflowController(EditorDocumentWorkspace& workspace,
        ProjectReadinessSession& readiness, PackageStateCatalog& catalog,
        PackageModuleWorkflow& workflow,
        ScriptAnalysisCoordinator& analysis,
        ScriptProjectionSession& projection,
        IEditorDiagnosticsService& diagnostics,
        std::unique_ptr<IPackageWorkflowToolAdapter> adapter = {});
    ~PackageWorkflowController();

    void Select(PackageWorkflowSelection selection);
    [[nodiscard]] PackageWorkflowResult OpenOrRefresh();
    [[nodiscard]] PackageWorkflowResult PrepareSelected(PackageModuleOperation operation);
    [[nodiscard]] PackageWorkflowResult Apply();
    [[nodiscard]] PackageWorkflowResult Cancel();
    [[nodiscard]] PackageWorkflowResult RetryRefresh();
    [[nodiscard]] PackageWorkflowResult PrepareUndo();
    [[nodiscard]] PackageWorkflowResult PrepareRedo();
    [[nodiscard]] std::vector<PackageRecoveryInspection> InspectRecovery();
    [[nodiscard]] PackageWorkflowResult RetryRecovery(
        const std::filesystem::path& journalPath);
    void RequestCancellation() noexcept;

    [[nodiscard]] PackageWorkflowControllerSnapshot Snapshot() const;
    [[nodiscard]] ChangeSubscription Subscribe(ChangeCallback callback);
    bool Unsubscribe(ChangeSubscription subscription);

private:
    [[nodiscard]] PackageWorkflowResult Prepare(
        PackageModuleOperation operation, PackageHistoryIntent historyIntent,
        PackageWorkflowSelection selection);
    [[nodiscard]] PackageRefreshResult RefreshDerived(
        const PackageWorkflowCapture& committedCapture);
    [[nodiscard]] std::pair<PackageWorkflowCapture, PackageWorkflowResult>
        Capture() const;
    void SetState(PackageWorkflowControllerState state,
        std::string progress = {});
    void Finish(PackageWorkflowResult result);
    void PublishDiagnostic(const PackageWorkflowResult& result,
        const std::vector<PackageUsageLocation>& locations = {});
    void Notify();
    [[nodiscard]] bool BeginOperation(PackageWorkflowControllerState state);

    EditorDocumentWorkspace& workspace_;
    ProjectReadinessSession& readiness_;
    PackageStateCatalog& catalog_;
    PackageModuleWorkflow& workflow_;
    ScriptAnalysisCoordinator& analysis_;
    ScriptProjectionSession& projection_;
    IEditorDiagnosticsService& diagnostics_;
    std::unique_ptr<IPackageWorkflowToolAdapter> adapter_;
    PackageWorkflowControllerSnapshot snapshot_;
    std::shared_ptr<std::atomic_bool> cancellation_;
    std::optional<PackageWorkflowCapture> committedCapture_;
    std::vector<PackageUsageLocation> pendingUsageLocations_;
    std::map<ChangeSubscription, ChangeCallback> callbacks_;
    ChangeSubscription nextSubscription_ = 1;
    mutable std::recursive_mutex stateMutex_;
};
}
