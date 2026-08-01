// SPDX-License-Identifier: MPL-2.0

#include "SagaEditor/Packages/PackageAuthoring.h"

#include <gtest/gtest.h>

#include <chrono>
#include <fstream>

using namespace SagaEditor;

namespace
{
namespace fs = std::filesystem;

fs::path WorkflowRoot(std::string_view name, bool instrumentation = false)
{
    const auto root = fs::temp_directory_path() /
        ("saga-package-workflow-" + std::string(name) + "-" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(root);
    std::ofstream(root / "Fixture.sagaproj", std::ios::binary)
        << "{\n  \"dependencies\": [{\"packageId\":\"pkg\",\"version\":\"1\","
           "\"modules\":[\"core\""
        << (instrumentation ? ",\"instrumentation\"" : "")
        << "]}]\n}\n";
    std::ofstream(root / "SagaPackage.lock", std::ios::binary) << "lock-base\n";
    return root;
}

std::string FileBytes(const fs::path& path)
{
    std::ifstream input(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(input),
            std::istreambuf_iterator<char>()};
}

PackageWorkflowCapture Capture(const fs::path& root)
{
    return {root, root / "Fixture.sagaproj", root / "SagaPackage.lock",
        FileBytes(root / "Fixture.sagaproj"), FileBytes(root / "SagaPackage.lock"),
        "sha256:sources", "generation-1", "sha256:generation",
        "sha256:graph", "sha256:ownership"};
}

std::string Inspection(bool enabled)
{
    return std::string(R"({
  "schemaVersion": 1,
  "status": "Passed",
  "offline": true,
  "graphHash": "sha256:candidate-graph",
  "generationId": "candidate-generation",
  "modules": [{
    "packageId": "pkg",
    "version": "1",
    "module": "instrumentation",
    "enabled": )") + (enabled ? "true" : "false") + R"(,
    "runtimeReadiness": "Ready",
    "artifactKinds": ["apiSurface"],
    "apiSurfaces": ["pkg/instrumentation"],
    "blocksSurfaces": ["pkg/instrumentation"],
    "capabilities": ["DevelopmentDiagnostics"],
    "license": "MPL-2.0",
    "trust": "TrustedEngineDistribution",
    "origin": "fixture",
    "offline": true
  }]
})";
}

PackageCandidateResolution CandidateResolution(bool enabled = true)
{
    return {true, {}, {}, "lock-candidate\n", Inspection(enabled),
        "sha256:candidate-graph"};
}
}

TEST(DependencyIntentEditorTests, PreservesUnknownBytesAndSortsOnlyModuleSpan)
{
    const std::string input =
        "{\r\n"
        "  \"unknown\": {\"sentinel\": \"keep\"},\r\n"
        "  \"dependencies\": [{\"packageId\":\"pkg\",\"unknown\":7,"
        "\"modules\":[\"zeta\", \"alpha\"],\"version\":\"1\"}]\r\n"
        "}\r\n";
    const auto result = DependencyIntentEditor::Edit(input, "pkg", "middle",
        PackageModuleOperation::Enable);
    ASSERT_EQ(result.status, DependencyIntentEditStatus::Changed);
    EXPECT_NE(result.candidateBytes.find(
        "\"unknown\": {\"sentinel\": \"keep\"}"), std::string::npos);
    EXPECT_NE(result.candidateBytes.find("\"unknown\":7"), std::string::npos);
    EXPECT_NE(result.candidateBytes.find(
        "\"modules\":[\"alpha\",\"middle\",\"zeta\"]"), std::string::npos);
    EXPECT_EQ(result.candidateBytes.find('\r'), std::string::npos);
    EXPECT_EQ(result.candidateBytes,
        "{\n"
        "  \"unknown\": {\"sentinel\": \"keep\"},\n"
        "  \"dependencies\": [{\"packageId\":\"pkg\",\"unknown\":7,"
        "\"modules\":[\"alpha\",\"middle\",\"zeta\"],\"version\":\"1\"}]\n"
        "}\n");
}

TEST(DependencyIntentEditorTests, SemanticNoOpsReturnNoChanges)
{
    const std::string input =
        "{\"dependencies\":[{\"packageId\":\"pkg\",\"version\":\"1\","
        "\"modules\":[\"enabled\"]}]}";
    EXPECT_EQ(DependencyIntentEditor::Edit(input, "pkg", "enabled",
        PackageModuleOperation::Enable).status,
        DependencyIntentEditStatus::NoChanges);
    EXPECT_EQ(DependencyIntentEditor::Edit(input, "pkg", "absent",
        PackageModuleOperation::Disable).status,
        DependencyIntentEditStatus::NoChanges);
}

TEST(ProjectReadinessSessionTests, AppliesRequiredBlockingPrecedence)
{
    ProjectReadinessSession session;
    EXPECT_EQ(session.State(), ProjectReadinessState::Ready);
    session.SetConformanceFailed(true);
    session.SetArtifactsOutOfDate(true);
    session.SetToolingOutOfDate(true);
    session.SetDirty(true);
    session.SetMutating(true);
    session.SetInvalid(true);
    session.SetRecoveryRequired(true);
    EXPECT_EQ(session.State(), ProjectReadinessState::RecoveryRequired);
    session.SetRecoveryRequired(false);
    EXPECT_EQ(session.State(), ProjectReadinessState::Invalid);
    session.SetInvalid(false);
    EXPECT_EQ(session.State(), ProjectReadinessState::Mutating);
    session.SetMutating(false);
    EXPECT_EQ(session.State(), ProjectReadinessState::Dirty);
    session.SetDirty(false);
    EXPECT_EQ(session.State(), ProjectReadinessState::ToolingOutOfDate);
    session.SetToolingOutOfDate(false);
    EXPECT_EQ(session.State(), ProjectReadinessState::ArtifactsOutOfDate);
    session.SetArtifactsOutOfDate(false);
    EXPECT_EQ(session.State(), ProjectReadinessState::ConformanceFailed);
}

TEST(PackageTransactionReviewTests, NormalizesSemanticLists)
{
    PackageTransactionReview review;
    review.moduleChanges = {"z", "a", "z"};
    review.capabilities = {"B", "A", "B"};
    review.refreshPlan = {"Script analysis", "Generation refresh",
        "Generation refresh"};
    review.Normalize();
    EXPECT_EQ(review.moduleChanges,
              (std::vector<std::string>{"a", "z"}));
    EXPECT_EQ(review.capabilities,
              (std::vector<std::string>{"A", "B"}));
    EXPECT_EQ(review.refreshPlan,
              (std::vector<std::string>{"Generation refresh",
                                        "Script analysis"}));
}

TEST(PackageModuleWorkflowTests, DirtyPrepareRejectsBeforeResolverOrLease)
{
    const auto root = WorkflowRoot("dirty");
    ProjectReadinessSession readiness;
    PackageStateCatalog catalog;
    PackageModuleWorkflow workflow(readiness, catalog);
    bool resolved = false;
    PackageWorkflowPrepareRequest request;
    request.capture = Capture(root);
    request.operation = PackageModuleOperation::Enable;
    request.packageId = "pkg";
    request.module = "instrumentation";
    request.relevantBuffersDirty = true;
    request.recapture = [&] { return Capture(root); };
    request.resolveCandidate = [&](const auto&, const auto&)
    {
        resolved = true;
        return CandidateResolution();
    };
    const auto result = workflow.Prepare(request);
    EXPECT_EQ(result.code, "Project.PackageTransaction.SaveAllRequired");
    EXPECT_FALSE(resolved);
    EXPECT_EQ(readiness.State(), ProjectReadinessState::Ready);
    fs::remove_all(root);
}

TEST(PackageModuleWorkflowTests, PrepareHoldsLeaseAndCancelPreservesCanonicalBytes)
{
    const auto root = WorkflowRoot("cancel");
    const auto before = Capture(root);
    ProjectReadinessSession readiness;
    PackageStateCatalog catalog;
    PackageModuleWorkflow workflow(readiness, catalog);
    PackageWorkflowPrepareRequest request;
    request.capture = before;
    request.operation = PackageModuleOperation::Enable;
    request.packageId = "pkg";
    request.module = "instrumentation";
    request.recapture = [&] { return Capture(root); };
    request.resolveCandidate = [](const auto& staged, const auto&)
    {
        EXPECT_TRUE(fs::is_regular_file(staged));
        return CandidateResolution();
    };
    const auto prepared = workflow.Prepare(request);
    ASSERT_EQ(prepared.status, PackageWorkflowStatus::Prepared);
    EXPECT_EQ(readiness.State(), ProjectReadinessState::Mutating);
    auto [competing, contention] = ProjectMutationLease::Acquire(root, "other");
    EXPECT_FALSE(contention.succeeded);
    EXPECT_EQ(workflow.Cancel().status, PackageWorkflowStatus::Cancelled);
    EXPECT_EQ(Capture(root).manifestBytes, before.manifestBytes);
    EXPECT_EQ(Capture(root).lockBytes, before.lockBytes);
    EXPECT_EQ(readiness.State(), ProjectReadinessState::Ready);
    fs::remove_all(root);
}

TEST(PackageModuleWorkflowTests, DisableWithSemanticUsageNeverStagesCanonicalMutation)
{
    const auto root = WorkflowRoot("usage", true);
    const auto before = Capture(root);
    ProjectReadinessSession readiness;
    PackageStateCatalog catalog;
    PackageModuleWorkflow workflow(readiness, catalog);
    PackageWorkflowPrepareRequest request;
    request.capture = before;
    request.operation = PackageModuleOperation::Disable;
    request.packageId = "pkg";
    request.module = "instrumentation";
    request.recapture = [&] { return Capture(root); };
    request.resolveCandidate = [](const auto&, const auto&)
        { return CandidateResolution(false); };
    request.analyzeUsage = [before](auto, auto, const auto&)
    {
        return PackageUsageAnalysis{true, {}, {}, before.sourceInventoryHash,
            before.generationId, before.generationHash, before.graphHash,
            before.ownershipCatalogHash, {"Scripts/Use.cs:7:21"}};
    };
    const auto result = workflow.Prepare(request);
    EXPECT_EQ(result.code, "Project.PackageTransaction.ModuleInUse");
    ASSERT_TRUE(result.review.has_value());
    EXPECT_TRUE(result.review->blocked);
    EXPECT_EQ(Capture(root).manifestBytes, before.manifestBytes);
    EXPECT_EQ(Capture(root).lockBytes, before.lockBytes);
    EXPECT_EQ(readiness.State(), ProjectReadinessState::Ready);
    fs::remove_all(root);
}

TEST(PackageModuleWorkflowTests, StaleSourceRejectsApplyAndInvalidatesReview)
{
    const auto root = WorkflowRoot("stale");
    const auto before = Capture(root);
    ProjectReadinessSession readiness;
    PackageStateCatalog catalog;
    PackageModuleWorkflow workflow(readiness, catalog);
    PackageWorkflowPrepareRequest request;
    request.capture = before;
    request.operation = PackageModuleOperation::Enable;
    request.packageId = "pkg";
    request.module = "instrumentation";
    request.recapture = [&] { return Capture(root); };
    request.resolveCandidate = [](const auto&, const auto&)
        { return CandidateResolution(); };
    const auto prepared = workflow.Prepare(request);
    ASSERT_TRUE(prepared.review.has_value());
    auto stale = before;
    stale.sourceInventoryHash = "sha256:changed";
    const auto result = workflow.Apply(prepared.review->transactionId,
        [stale] { return stale; }, {});
    EXPECT_EQ(result.code, "Project.PackageTransaction.StaleUsageAnalysis");
    EXPECT_FALSE(workflow.Active());
    EXPECT_EQ(Capture(root).manifestBytes, before.manifestBytes);
    EXPECT_EQ(readiness.State(), ProjectReadinessState::Ready);
    fs::remove_all(root);
}

TEST(PackageModuleWorkflowTests, CommittedInputsSurviveRefreshFailureAndRetryIsDerivedOnly)
{
    const auto root = WorkflowRoot("retry");
    ProjectReadinessSession readiness;
    PackageStateCatalog catalog;
    PackageModuleWorkflow workflow(readiness, catalog);
    PackageWorkflowPrepareRequest request;
    request.capture = Capture(root);
    request.operation = PackageModuleOperation::Enable;
    request.packageId = "pkg";
    request.module = "instrumentation";
    request.recapture = [&] { return Capture(root); };
    request.resolveCandidate = [](const auto&, const auto&)
        { return CandidateResolution(); };
    const auto prepared = workflow.Prepare(request);
    ASSERT_TRUE(prepared.review.has_value());
    const auto transactionId = prepared.review->transactionId;
    const auto applied = workflow.Apply(transactionId,
        [&] { return Capture(root); }, []
        {
            return PackageRefreshResult{false,
                "Project.PackageTransaction.GenerationRefreshFailed", "fixture", {}};
        });
    EXPECT_EQ(applied.status, PackageWorkflowStatus::ToolingOutOfDate);
    EXPECT_EQ(readiness.State(), ProjectReadinessState::ToolingOutOfDate);
    const auto committedManifest = FileBytes(root / "Fixture.sagaproj");
    const auto committedLock = FileBytes(root / "SagaPackage.lock");
    EXPECT_NE(committedManifest.find("instrumentation"), std::string::npos);
    EXPECT_EQ(committedLock, "lock-candidate\n");
    EXPECT_EQ(workflow.Apply(transactionId, [&] { return Capture(root); }, {}).code,
              "Project.PackageTransaction.AlreadyApplied");

    const auto retried = workflow.RetryRefresh([]
        { return PackageRefreshResult{true, {}, {}, Inspection(true)}; });
    EXPECT_EQ(retried.status, PackageWorkflowStatus::Applied);
    EXPECT_EQ(FileBytes(root / "Fixture.sagaproj"), committedManifest);
    EXPECT_EQ(FileBytes(root / "SagaPackage.lock"), committedLock);
    EXPECT_EQ(readiness.State(), ProjectReadinessState::Ready);
    ASSERT_NE(catalog.Find("pkg", "instrumentation"), nullptr);
    EXPECT_TRUE(catalog.Find("pkg", "instrumentation")->enabled);
    fs::remove_all(root);
}

TEST(PackageModuleWorkflowTests, UndoAndRedoMoveCursorWithoutAppendingRawHistory)
{
    const auto root = WorkflowRoot("history");
    ProjectReadinessSession readiness;
    PackageStateCatalog catalog;
    PackageModuleWorkflow workflow(readiness, catalog);
    const auto prepare = [&](PackageModuleOperation operation,
                             PackageHistoryIntent intent, bool enabled)
    {
        PackageWorkflowPrepareRequest request;
        request.capture = Capture(root);
        request.operation = operation;
        request.packageId = "pkg";
        request.module = "instrumentation";
        request.historyIntent = intent;
        request.recapture = [&] { return Capture(root); };
        request.resolveCandidate = [enabled](const auto&, const auto&)
        { return CandidateResolution(enabled); };
        request.analyzeUsage = [capture = request.capture](auto, auto, const auto&)
        {
            return PackageUsageAnalysis{true, {}, {},
                capture.sourceInventoryHash, capture.generationId,
                capture.generationHash, capture.graphHash,
                capture.ownershipCatalogHash, {}};
        };
        return workflow.Prepare(request);
    };
    const auto apply = [&](const PackageWorkflowResult& prepared, bool enabled)
    {
        if (!prepared.review)
            return PackageWorkflowResult{PackageWorkflowStatus::Rejected,
                "missing-review", {}, {}};
        return workflow.Apply(prepared.review->transactionId,
            [&] { return Capture(root); }, [enabled]
            { return PackageRefreshResult{true, {}, {}, Inspection(enabled)}; });
    };

    const auto forward = prepare(PackageModuleOperation::Enable,
        PackageHistoryIntent::Forward, true);
    EXPECT_EQ(apply(forward, true).status, PackageWorkflowStatus::Applied);
    ASSERT_EQ(workflow.History().size(), 1U);
    ASSERT_TRUE(workflow.UndoIntent().has_value());
    EXPECT_FALSE(workflow.RedoIntent().has_value());

    const auto undo = prepare(PackageModuleOperation::Disable,
        PackageHistoryIntent::Undo, false);
    EXPECT_EQ(apply(undo, false).status, PackageWorkflowStatus::Applied);
    EXPECT_EQ(workflow.History().size(), 1U);
    EXPECT_FALSE(workflow.UndoIntent().has_value());
    ASSERT_TRUE(workflow.RedoIntent().has_value());
    EXPECT_EQ(workflow.RedoIntent()->operation, PackageModuleOperation::Enable);

    const auto redo = prepare(PackageModuleOperation::Enable,
        PackageHistoryIntent::Redo, true);
    EXPECT_EQ(apply(redo, true).status, PackageWorkflowStatus::Applied);
    EXPECT_EQ(workflow.History().size(), 1U);
    EXPECT_TRUE(workflow.UndoIntent().has_value());
    EXPECT_FALSE(workflow.RedoIntent().has_value());
    fs::remove_all(root);
}
