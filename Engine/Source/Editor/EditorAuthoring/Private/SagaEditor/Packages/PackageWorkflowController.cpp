// SPDX-License-Identifier: MPL-2.0

#include "SagaEditor/Packages/PackageWorkflowController.h"

#include "SagaEditor/Diagnostics/IEditorDiagnosticsService.h"
#include "SagaEditor/Documents/SourceDocuments.h"
#include "SagaEditor/Scripting/ScriptAnalysisCoordinator.h"
#include "SagaEditor/Scripting/ScriptProjectionSession.h"
#include "SagaEditor/Workspace/EditorDocumentWorkspace.h"

#include <algorithm>

namespace SagaEditor
{
namespace
{
constexpr std::string_view kDiagnosticSource = "editor.packages";

std::string PhaseName(ProjectMutationPhase phase)
{
    switch (phase)
    {
    case ProjectMutationPhase::Preparing: return "Preparing";
    case ProjectMutationPhase::Resolved: return "Resolved";
    case ProjectMutationPhase::Reviewed: return "Reviewed";
    case ProjectMutationPhase::Staging: return "Staging";
    case ProjectMutationPhase::CommitMarked: return "CommitMarked";
    case ProjectMutationPhase::Publishing: return "Publishing";
    case ProjectMutationPhase::Committed: return "Committed";
    case ProjectMutationPhase::Cancelled: return "Cancelled";
    case ProjectMutationPhase::RolledBack: return "RolledBack";
    case ProjectMutationPhase::RecoveryRequired: return "RecoveryRequired";
    }
    return "RecoveryRequired";
}

PackageWorkflowResult Rejected(std::string code, std::string message)
{
    return {PackageWorkflowStatus::Rejected, std::move(code),
        std::move(message), {}};
}

std::string HashFile(const std::filesystem::path& path)
{
    std::error_code error;
    const auto bytes = ReadDocumentBytes(path, error);
    return error ? std::string{} : HashDocumentBytes(bytes);
}
}

PackageWorkflowController::PackageWorkflowController(
    EditorDocumentWorkspace& workspace, ProjectReadinessSession& readiness,
    PackageStateCatalog& catalog, PackageModuleWorkflow& workflow,
    ScriptAnalysisCoordinator& analysis, ScriptProjectionSession& projection,
    IEditorDiagnosticsService& diagnostics,
    std::unique_ptr<IPackageWorkflowToolAdapter> adapter)
    : workspace_(workspace), readiness_(readiness), catalog_(catalog),
      workflow_(workflow), analysis_(analysis), projection_(projection),
      diagnostics_(diagnostics), adapter_(std::move(adapter))
{
    if (!adapter_)
        adapter_ = std::make_unique<ManagedPackageWorkflowToolAdapter>();
    cancellation_ = std::make_shared<std::atomic_bool>(false);
    if (workspace_.HasRecoveryRequired())
    {
        readiness_.SetRecoveryRequired(true);
        snapshot_.state = PackageWorkflowControllerState::RecoveryRequired;
        snapshot_.recovery = InspectRecovery();
    }
}

PackageWorkflowController::~PackageWorkflowController()
{
    RequestCancellation();
}

void PackageWorkflowController::Select(PackageWorkflowSelection selection)
{
    const std::lock_guard lock(stateMutex_);
    snapshot_.selection = std::move(selection);
    Notify();
}

bool PackageWorkflowController::BeginOperation(
    PackageWorkflowControllerState state)
{
    const std::lock_guard lock(stateMutex_);
    if (snapshot_.active || workflow_.Active()) return false;
    cancellation_ = std::make_shared<std::atomic_bool>(false);
    snapshot_.active = true;
    readiness_.SetMutating(true);
    snapshot_.review.reset();
    snapshot_.code.clear();
    snapshot_.message.clear();
    pendingUsageLocations_.clear();
    SetState(state);
    return true;
}

std::pair<PackageWorkflowCapture, PackageWorkflowResult>
PackageWorkflowController::Capture() const
{
    return adapter_->Capture(workspace_);
}

PackageWorkflowResult PackageWorkflowController::OpenOrRefresh()
{
    if (!BeginOperation(PackageWorkflowControllerState::Preparing))
        return Rejected("editor.packages.reentrant",
            "Another package operation is active.");
    if (workspace_.HasRecoveryRequired())
    {
        readiness_.SetRecoveryRequired(true);
        snapshot_.recovery = InspectRecovery();
        auto result = Rejected("editor.packages.recovery-required",
            "Package transaction recovery must complete before refresh.");
        Finish(result);
        return result;
    }
    auto [capture, captured] = Capture();
    if (captured.status == PackageWorkflowStatus::Rejected)
    {
        catalog_.Invalidate();
        projection_.PackageGenerationChanged({}, {});
        auto refreshed = RefreshDerived(capture);
        PackageWorkflowResult result;
        result.status = refreshed.passed ? PackageWorkflowStatus::Applied
                                         : PackageWorkflowStatus::ToolingOutOfDate;
        result.code = refreshed.code;
        result.message = refreshed.message;
        Finish(result);
        return result;
    }
    auto refreshed = RefreshDerived(capture);
    PackageWorkflowResult result;
    result.status = refreshed.passed ? PackageWorkflowStatus::Applied
                                     : PackageWorkflowStatus::ToolingOutOfDate;
    result.code = refreshed.code;
    result.message = refreshed.message;
    Finish(result);
    return result;
}

PackageWorkflowResult PackageWorkflowController::PrepareSelected(
    PackageModuleOperation operation)
{
    return Prepare(operation, PackageHistoryIntent::Forward,
        snapshot_.selection);
}

PackageWorkflowResult PackageWorkflowController::PrepareUndo()
{
    const auto intent = workflow_.UndoIntent();
    if (!intent) return Rejected("editor.packages.undo-unavailable",
        "There is no package intent to undo.");
    return Prepare(intent->operation, PackageHistoryIntent::Undo,
        {intent->packageId, intent->module});
}

PackageWorkflowResult PackageWorkflowController::PrepareRedo()
{
    const auto intent = workflow_.RedoIntent();
    if (!intent) return Rejected("editor.packages.redo-unavailable",
        "There is no package intent to redo.");
    return Prepare(intent->operation, PackageHistoryIntent::Redo,
        {intent->packageId, intent->module});
}

PackageWorkflowResult PackageWorkflowController::Prepare(
    PackageModuleOperation operation, PackageHistoryIntent historyIntent,
    PackageWorkflowSelection selection)
{
    if (!selection.Valid()) return Rejected("editor.packages.selection-required",
        "Select a package module first.");
    if (!BeginOperation(PackageWorkflowControllerState::Preparing))
        return Rejected("editor.packages.reentrant",
            "Another package operation or review is active.");
    auto [capture, captured] = Capture();
    if (captured.status == PackageWorkflowStatus::Rejected)
    {
        Finish(captured);
        return captured;
    }
    if (!capture.derivedStateCurrent)
    {
        auto result = Rejected("editor.packages.tooling-out-of-date",
            "Refresh package tooling before preparing a dependency review.");
        readiness_.SetToolingOutOfDate(true);
        Finish(result);
        return result;
    }
    PackageWorkflowPrepareRequest request;
    request.capture = capture;
    request.operation = operation;
    request.packageId = selection.packageId;
    request.module = selection.module;
    request.historyIntent = historyIntent;
    request.relevantBuffersDirty = !workspace_.DirtyDocumentIds().empty();
    request.recapture = [this]
    {
        return Capture().first;
    };
    request.resolveCandidate = [this, capture](const auto& staged,
        const auto& stagingRoot)
    {
        SetState(PackageWorkflowControllerState::Resolving, "Resolving");
        return adapter_->ResolveCandidate(capture, staged, stagingRoot,
            {cancellation_, [this](std::string_view progress)
            { SetState(PackageWorkflowControllerState::Resolving,
                std::string(progress)); }});
    };
    request.analyzeUsage = [this, capture](std::string_view packageId,
        std::string_view module, const std::filesystem::path& stagingRoot)
    {
        SetState(PackageWorkflowControllerState::Analyzing, "Analyzing usage");
        auto usage = adapter_->AnalyzeUsage(capture, packageId, module, stagingRoot,
            {cancellation_, [this](std::string_view progress)
            { SetState(PackageWorkflowControllerState::Analyzing,
                std::string(progress)); }});
        pendingUsageLocations_ = usage.locations;
        return usage;
    };
    auto result = workflow_.Prepare(request);
    {
        const std::lock_guard lock(stateMutex_);
        snapshot_.active = false;
        if (result.status == PackageWorkflowStatus::Prepared && result.review)
        {
            snapshot_.review = result.review;
            SetState(PackageWorkflowControllerState::AwaitingReview,
                "Review required");
        }
        else
        {
            Finish(result);
        }
    }
    PublishDiagnostic(result, pendingUsageLocations_);
    return result;
}

PackageWorkflowResult PackageWorkflowController::Apply()
{
    std::string transactionId;
    {
        const std::lock_guard lock(stateMutex_);
        if (snapshot_.active)
            return Rejected("editor.packages.reentrant",
                "Another package operation is active.");
        if (!snapshot_.review || !workflow_.Active())
            return Rejected("editor.packages.review-required",
                "Apply requires the active reviewed transaction.");
        if (snapshot_.review->blocked || snapshot_.review->stale)
            return Rejected("editor.packages.review-blocked",
                "The active review is blocked or stale.");
        snapshot_.active = true;
        transactionId = snapshot_.review->transactionId;
    }
    SetState(PackageWorkflowControllerState::Applying, "Publishing inputs");
    auto result = workflow_.Apply(transactionId,
        [this] { return Capture().first; },
        [this]
        {
            auto [capture, captured] = Capture();
            if (captured.status == PackageWorkflowStatus::Rejected)
                return PackageRefreshResult{false, captured.code,
                    captured.message, {}};
            committedCapture_ = capture;
            return RefreshDerived(capture);
        });
    {
        const std::lock_guard lock(stateMutex_);
        snapshot_.review.reset();
    }
    Finish(result);
    return result;
}

PackageWorkflowResult PackageWorkflowController::Cancel()
{
    {
        const std::lock_guard lock(stateMutex_);
        if (snapshot_.state == PackageWorkflowControllerState::Applying ||
            snapshot_.state == PackageWorkflowControllerState::Refreshing)
            return Rejected("editor.packages.cancellation-too-late",
                "Publication has started and must reach a safe terminal state.");
    }
    RequestCancellation();
    auto result = workflow_.Active() ? workflow_.Cancel()
        : PackageWorkflowResult{PackageWorkflowStatus::Cancelled, {}, {}, {}};
    {
        const std::lock_guard lock(stateMutex_);
        snapshot_.review.reset();
    }
    Finish(result);
    return result;
}

PackageWorkflowResult PackageWorkflowController::RetryRefresh()
{
    if (!BeginOperation(PackageWorkflowControllerState::Refreshing))
        return Rejected("editor.packages.reentrant",
            "Another package operation is active.");
    auto [before, captured] = Capture();
    if (captured.status == PackageWorkflowStatus::Rejected)
    {
        if (!committedCapture_)
        {
            Finish(captured);
            return captured;
        }
        before = *committedCapture_;
    }
    const auto manifestHash = before.manifestHash;
    const auto lockHash = before.lockHash;
    auto result = workflow_.RetryRefresh([this, before]
        { return RefreshDerived(before); });
    if (HashFile(before.manifestPath) != manifestHash ||
        HashFile(before.lockPath) != lockHash)
    {
        result = Rejected("editor.packages.retry-mutated-inputs",
            "Derived-only retry changed canonical package inputs.");
        readiness_.SetRecoveryRequired(true);
    }
    Finish(result);
    return result;
}

PackageRefreshResult PackageWorkflowController::RefreshDerived(
    const PackageWorkflowCapture& committedCapture)
{
    SetState(PackageWorkflowControllerState::Refreshing,
        "Refreshing package generation");
    catalog_.Invalidate();
    projection_.PackageGenerationChanged({}, {});
    const PackageToolOperationContext context{cancellation_,
        [this](std::string_view progress)
        { SetState(PackageWorkflowControllerState::Refreshing,
            std::string(progress)); }};
    const auto refreshed = adapter_->RefreshGeneration(committedCapture, context);
    if (!refreshed.passed)
    {
        readiness_.SetToolingOutOfDate(true);
        return {false, refreshed.code, refreshed.message, {}};
    }
    auto [current, captured] = Capture();
    if (captured.status == PackageWorkflowStatus::Rejected)
        return {false, captured.code, captured.message, {}};
    SetState(PackageWorkflowControllerState::Analyzing, "Analyzing saved source");
    ScriptProjectionRequestIdentity identity;
    identity.workspaceGeneration = workspace_.Generation();
    identity.savedDiskHash = current.sourceInventoryHash;
    identity.documentRevision = workspace_.Generation();
    identity.projectHash = current.projectHash;
    identity.packageGenerationId = current.generationId;
    identity.packageGenerationHash = current.generationHash;
    identity.operationId = "package-refresh-" + current.generationId;
    projection_.Begin(identity);
    ScriptAnalysisRequest analysisRequest;
    analysisRequest.progress = [this](std::string_view progress)
    { SetState(PackageWorkflowControllerState::Analyzing, std::string(progress)); };
    const auto analysis = analysis_.Analyze(workspace_, analysisRequest);
    if (!analysis.passed || !analysis.projection ||
        !projection_.Publish(identity, analysis.projectionArtifactHash,
            analysis.projection))
    {
        std::string message = "Script analysis or shared projection publication failed.";
        if (!analysis.diagnostics.empty()) message = analysis.diagnostics.front().message;
        projection_.Fail(identity, message);
        readiness_.SetToolingOutOfDate(true);
        return {false, "editor.packages.analysis-refresh-failed", message, {}};
    }
    std::string catalogDiagnostic;
    if (!catalog_.LoadInspection(refreshed.inspectionBytes, catalogDiagnostic))
    {
        readiness_.SetToolingOutOfDate(true);
        return {false, "editor.packages.catalog-refresh-failed",
            catalogDiagnostic, {}};
    }
    readiness_.SetToolingOutOfDate(false);
    committedCapture_ = current;
    return {true, {}, {}, refreshed.inspectionBytes};
}

std::vector<PackageRecoveryInspection>
PackageWorkflowController::InspectRecovery()
{
    const std::lock_guard lock(stateMutex_);
    std::vector<PackageRecoveryInspection> inspections;
    for (const auto& recovery : workspace_.PackageRecovery())
    {
        PackageRecoveryInspection inspection;
        inspection.journalPath = recovery.journal;
        inspection.code = recovery.code;
        inspection.message = recovery.message;
        const auto [journal, loaded] = ProjectMutationJournal::Load(recovery.journal);
        if (journal)
        {
            inspection.schemaVersion = journal->schemaVersion;
            inspection.transactionId = journal->transactionId;
            inspection.operationId = journal->operationId;
            inspection.phase = PhaseName(journal->phase);
            inspection.commitMarker = journal->commitMarker;
            inspection.files = journal->files;
        }
        else if (inspection.message.empty()) inspection.message = loaded.message;
        inspections.push_back(std::move(inspection));
    }
    snapshot_.recovery = inspections;
    return inspections;
}

PackageWorkflowResult PackageWorkflowController::RetryRecovery(
    const std::filesystem::path& journalPath)
{
    if (!BeginOperation(PackageWorkflowControllerState::RecoveryRequired))
        return Rejected("editor.packages.reentrant",
            "Another package operation is active.");
    if (!workspace_.Project())
    {
        auto result = Rejected("editor.packages.project-not-open",
            "A project must be open for recovery.");
        Finish(result);
        return result;
    }
    const auto recovered = ProjectMutationRecovery::Recover(
        workspace_.Project()->rootPath, journalPath);
    if (recovered.action == ProjectMutationRecoveryAction::RecoveryRequired ||
        recovered.action == ProjectMutationRecoveryAction::UnsupportedJournal)
    {
        readiness_.SetRecoveryRequired(true);
        auto result = Rejected("editor.packages.recovery-required",
            recovered.message);
        Finish(result);
        return result;
    }
    readiness_.SetRecoveryRequired(false);
    {
        const std::lock_guard lock(stateMutex_);
        snapshot_.recovery.clear();
    }
    auto [capture, captured] = Capture();
    PackageWorkflowResult result;
    if (captured.status == PackageWorkflowStatus::Rejected)
        result = captured;
    else
    {
        const auto refresh = RefreshDerived(capture);
        result.status = refresh.passed ? PackageWorkflowStatus::Applied
                                       : PackageWorkflowStatus::ToolingOutOfDate;
        result.code = refresh.code;
        result.message = refresh.message;
    }
    Finish(result);
    return result;
}

void PackageWorkflowController::RequestCancellation() noexcept
{
    const std::lock_guard lock(stateMutex_);
    if (snapshot_.state == PackageWorkflowControllerState::Preparing ||
        snapshot_.state == PackageWorkflowControllerState::Resolving ||
        snapshot_.state == PackageWorkflowControllerState::Analyzing)
        cancellation_->store(true, std::memory_order_relaxed);
}

PackageWorkflowControllerSnapshot PackageWorkflowController::Snapshot() const
{
    const std::lock_guard lock(stateMutex_);
    return snapshot_;
}

PackageWorkflowController::ChangeSubscription
PackageWorkflowController::Subscribe(ChangeCallback callback)
{
    const std::lock_guard lock(stateMutex_);
    if (!callback) return InvalidChangeSubscription;
    const auto subscription = nextSubscription_++;
    callbacks_.emplace(subscription, std::move(callback));
    callbacks_.at(subscription)(snapshot_);
    return subscription;
}

bool PackageWorkflowController::Unsubscribe(ChangeSubscription subscription)
{
    const std::lock_guard lock(stateMutex_);
    return callbacks_.erase(subscription) != 0;
}

void PackageWorkflowController::SetState(PackageWorkflowControllerState state,
    std::string progress)
{
    const std::lock_guard lock(stateMutex_);
    snapshot_.state = state;
    snapshot_.progress = std::move(progress);
    Notify();
}

void PackageWorkflowController::Finish(PackageWorkflowResult result)
{
    const std::lock_guard lock(stateMutex_);
    snapshot_.active = false;
    readiness_.SetMutating(false);
    snapshot_.code = result.code;
    snapshot_.message = result.message;
    if (readiness_.State() == ProjectReadinessState::RecoveryRequired)
        snapshot_.state = PackageWorkflowControllerState::RecoveryRequired;
    else if (result.status == PackageWorkflowStatus::ToolingOutOfDate ||
             readiness_.State() == ProjectReadinessState::ToolingOutOfDate)
        snapshot_.state = PackageWorkflowControllerState::ToolingOutOfDate;
    else if (snapshot_.review)
        snapshot_.state = PackageWorkflowControllerState::AwaitingReview;
    else
        snapshot_.state = PackageWorkflowControllerState::Ready;
    PublishDiagnostic(result);
    Notify();
}

void PackageWorkflowController::PublishDiagnostic(
    const PackageWorkflowResult& result,
    const std::vector<PackageUsageLocation>& locations)
{
    if (result.status != PackageWorkflowStatus::Rejected &&
        result.status != PackageWorkflowStatus::ToolingOutOfDate)
    {
        diagnostics_.ClearSource(std::string(kDiagnosticSource));
        return;
    }
    std::vector<EditorDiagnostic> diagnostics;
    const auto append = [&](const PackageUsageLocation* location)
    {
        EditorDiagnostic diagnostic;
        diagnostic.severity = EditorDiagnosticSeverity::Error;
        diagnostic.source = std::string(kDiagnosticSource);
        diagnostic.code = result.code.empty()
            ? "editor.packages.operation-failed" : result.code;
        diagnostic.message = result.message.empty()
            ? "The package workflow did not complete." : result.message;
        diagnostic.publishBlocking = true;
        if (location)
        {
            diagnostic.location.resource = location->sourcePath.generic_string();
            diagnostic.location.line = location->startLine;
            diagnostic.location.column = location->startColumn;
        }
        diagnostics.push_back(std::move(diagnostic));
    };
    if (locations.empty()) append(nullptr);
    else for (const auto& location : locations) append(&location);
    diagnostics_.ReplaceSource(std::string(kDiagnosticSource),
        std::move(diagnostics));
}

void PackageWorkflowController::Notify()
{
    const std::lock_guard lock(stateMutex_);
    snapshot_.canApply = snapshot_.state ==
        PackageWorkflowControllerState::AwaitingReview && snapshot_.review &&
        !snapshot_.review->blocked && !snapshot_.review->stale;
    snapshot_.canCancel = snapshot_.state ==
        PackageWorkflowControllerState::AwaitingReview ||
        snapshot_.state == PackageWorkflowControllerState::Preparing ||
        snapshot_.state == PackageWorkflowControllerState::Resolving ||
        snapshot_.state == PackageWorkflowControllerState::Analyzing;
    snapshot_.canRetry = snapshot_.state ==
        PackageWorkflowControllerState::ToolingOutOfDate;
    snapshot_.canUndo = workflow_.UndoIntent().has_value() && !snapshot_.active &&
        snapshot_.state == PackageWorkflowControllerState::Ready;
    snapshot_.canRedo = workflow_.RedoIntent().has_value() && !snapshot_.active &&
        snapshot_.state == PackageWorkflowControllerState::Ready;
    const auto callbacks = callbacks_;
    for (const auto& [id, callback] : callbacks)
    {
        (void)id;
        if (callback) callback(snapshot_);
    }
}
}
