// SPDX-License-Identifier: MPL-2.0
#include "SagaProductTestSupport.hpp"

using namespace SagaProduct;
using namespace SagaProductTestSupport;

namespace
{
TEST(SagaProductWorkflowSmokeTest,
     AppEntrypointWritesStarterArenaReportWithoutRunningWorkflowTools)
{
    const fs::path root = MakeTempDir("saga_product_workflow_smoke_test");
    const fs::path reportPath =
        root / "starter_arena_product_shell_workflow_report.json";

    SagaProduct::SagaApp app;
    SagaAppConfig config;
    config.workflowSmoke = true;
    const fs::path manifest = SourceRoot() / "Samples" / "StarterArena" /
        "StarterArena.sagaproj";
    const std::string projectId = ProjectIdFromManifest(manifest);
    config.workflowProjectPath = manifest;
    config.workflowProfile = "project_readiness";
    config.workflowReportPath = reportPath;

    std::ostringstream out;
    std::ostringstream err;
    const int exitCode = app.Run(config, out, err);

    EXPECT_EQ(exitCode, 0);
    EXPECT_NE(out.str().find("workflow.report="), std::string::npos);
    EXPECT_NE(out.str().find("workflow.status=PassedWithLimitations"),
              std::string::npos);
    EXPECT_TRUE(err.str().empty());
    ASSERT_TRUE(fs::exists(reportPath));

    std::ifstream input(reportPath);
    const nlohmann::json report = nlohmann::json::parse(input);
    EXPECT_EQ(report["schemaVersion"], 2);
    EXPECT_EQ(report["tool"], "Saga");
    EXPECT_EQ(report["action"], "workflow-smoke");
    EXPECT_EQ(report["verified"], false);
    EXPECT_EQ(report["status"], "PassedWithLimitations");
    EXPECT_EQ(report["project"]["projectId"], projectId);
    EXPECT_EQ(report["profile"]["requestedProfileId"], "project_readiness");

    const std::vector<std::string> expectedActionIds = {
        "project_validation",
        "editor_inspection",
        "runtime_smoke",
        "sagascript_analyze_compile",
        "visual_blocks_cli_chain",
        "package_preflight",
        "known_limitations",
    };
    ASSERT_TRUE(report["workflowActions"].is_array());
    ASSERT_EQ(report["workflowActions"].size(), expectedActionIds.size());
    for (std::size_t index = 0; index < expectedActionIds.size(); ++index)
    {
        const nlohmann::json& action = report["workflowActions"][index];
        EXPECT_EQ(action["id"], expectedActionIds[index]);
        EXPECT_TRUE(action.contains("status"));
        EXPECT_TRUE(action.contains("actionKind"));
        EXPECT_TRUE(action.contains("owner"));
        EXPECT_TRUE(action.contains("expectedReportPath"));
        EXPECT_TRUE(action.contains("availability"));
    }

    const nlohmann::json* editorInspection =
        FindWorkflowAction(report, "editor_inspection");
    ASSERT_NE(editorInspection, nullptr);
    EXPECT_EQ((*editorInspection)["actionKind"], "editor-inspection");
    EXPECT_EQ((*editorInspection)["owner"], "Saga Editor Workspace");
    EXPECT_EQ((*editorInspection)["availability"], "available");

    const nlohmann::json* packagePreflight =
        FindWorkflowAction(report, "package_preflight");
    ASSERT_NE(packagePreflight, nullptr);
    EXPECT_EQ((*packagePreflight)["actionKind"], "package-preflight");
    EXPECT_EQ((*packagePreflight)["owner"], "package-linux-saga");
    EXPECT_EQ((*packagePreflight)["availability"], "preflight_only");
    EXPECT_EQ((*packagePreflight)["diagnosticId"],
              "Saga.Workflow.PackagePreflightOnly");

    ASSERT_TRUE(report["reportReferences"].is_array());
    EXPECT_EQ(report["reportReferences"].size(), expectedActionIds.size());
    EXPECT_FALSE(ContainsJsonKeyRecursively(report, "command"));
    EXPECT_FALSE(report.contains("workflowSteps"));
    EXPECT_FALSE(report.contains("commands"));

    const std::string serialized = report.dump();
    const std::vector<std::string> forbiddenReportContent = {
        "nix-shell --run",
        " && ",
        " --project ",
        " -> ",
        std::string("build/RelWithDebInfo-") + "0.0.9",
        std::string("server_") + "authority_smoke",
        std::string("MultiplayerSandbox") + "Headless",
        "SagaServer",
        "SagaClient",
    };
    for (const std::string& forbidden : forbiddenReportContent)
    {
        EXPECT_EQ(serialized.find(forbidden), std::string::npos)
            << "Forbidden workflow-smoke content: " << forbidden;
    }

    EXPECT_TRUE(JsonArrayContainsString(report["nonClaims"],
                                        "full product dashboard"));
    EXPECT_TRUE(JsonArrayContainsString(report["nonClaims"],
                                        "distribution readiness"));
}

TEST(SagaLocalWorkspaceTransactionSmokeTest,
     StarterArenaReportIsReadOnlyAndDoesNotClaimCollaborationRuntime)
{
    const fs::path root =
        MakeTempDir("saga_local_workspace_transaction_smoke_test");
    const fs::path reportPath =
        root / "starter_arena_local_workspace_transaction_report.json";
    const fs::path manifest = SourceRoot() / "Samples" / "StarterArena" /
        "StarterArena.sagaproj";
    const std::string projectId = ProjectIdFromManifest(manifest);
    const std::string manifestBefore = ReadFile(manifest);
    const auto manifestWriteTimeBefore = fs::last_write_time(manifest);

    SagaProduct::SagaApp app;
    SagaAppConfig config;
    config.localWorkspaceTransactionSmoke = true;
    config.workflowProjectPath = manifest;
    config.workspaceSelector = "builtin:basic";
    config.localWorkspaceActorId = "local.actor";
    config.localWorkspaceOperationKind = "InspectProject";
    config.localWorkspaceTransactionReportPath = reportPath;

    std::ostringstream out;
    std::ostringstream err;
    const int exitCode = app.Run(config, out, err);

    EXPECT_EQ(exitCode, 0);
    EXPECT_NE(out.str().find("transaction.report="), std::string::npos);
    EXPECT_NE(out.str().find("transaction.status=Ready"),
              std::string::npos);
    EXPECT_TRUE(err.str().empty());
    EXPECT_EQ(ReadFile(manifest), manifestBefore);
    EXPECT_EQ(fs::last_write_time(manifest), manifestWriteTimeBefore);
    ASSERT_TRUE(fs::exists(reportPath));

    std::ifstream input(reportPath);
    const nlohmann::json report = nlohmann::json::parse(input);
    EXPECT_EQ(report["tool"], "Saga");
    EXPECT_EQ(report["command"], "local-workspace-transaction-smoke");
    EXPECT_EQ(report["verified"], false);
    EXPECT_EQ(report["status"], "Ready");
    EXPECT_EQ(report["workspace"]["workspaceId"], "builtin.basic");
    EXPECT_EQ(report["workspace"]["selector"], "builtin:basic");
    EXPECT_EQ(report["project"]["projectId"], projectId);
    EXPECT_EQ(report["transaction"]["workspaceId"], "builtin.basic");
    EXPECT_EQ(report["transaction"]["projectId"], projectId);
    EXPECT_EQ(report["transaction"]["actorId"], "local.actor");
    EXPECT_EQ(report["transaction"]["operationKind"], "InspectProject");
    EXPECT_TRUE(report["transaction"]["readOnlyEvaluation"].get<bool>());
    EXPECT_EQ(report["transaction"]["status"], "ready");
    EXPECT_NE(report["transaction"]["transactionId"].get<std::string>().find(
                  "local-transaction:builtin.basic:" + projectId + ":local.actor:InspectProject"),
              std::string::npos);
    EXPECT_TRUE(JsonArrayContainsString(report["operationExamples"],
                                        "PlanScriptBlockEdit"));
    EXPECT_TRUE(JsonArrayContainsString(report["operationExamples"],
                                        "RunWorkflowSmokeReference"));
    EXPECT_TRUE(JsonArrayContainsString(report["nonClaims"],
                                        "full multiplayer collaboration"));
    EXPECT_TRUE(JsonArrayContainsString(report["nonClaims"],
                                        "collaboration server"));
    EXPECT_TRUE(JsonArrayContainsString(report["nonClaims"],
                                        "real-time team editing"));
    EXPECT_TRUE(JsonArrayContainsString(report["nonClaims"],
                                        "distribution readiness"));
}

TEST(SagaLocalPresenceLockSmokeTest,
     StarterArenaReportIsReadOnlyAndDoesNotClaimCollaborationRuntime)
{
    const fs::path root =
        MakeTempDir("saga_local_presence_lock_smoke_test");
    const fs::path reportPath =
        root / "starter_arena_presence_lock_report.json";
    const fs::path manifest = SourceRoot() / "Samples" / "StarterArena" /
        "StarterArena.sagaproj";
    const std::string projectId = ProjectIdFromManifest(manifest);
    const std::string manifestBefore = ReadFile(manifest);
    const auto manifestWriteTimeBefore = fs::last_write_time(manifest);

    SagaProduct::SagaApp app;
    SagaAppConfig config;
    config.localWorkspacePresenceLockSmoke = true;
    config.workflowProjectPath = manifest;
    config.workspaceSelector = "builtin:basic";
    config.localWorkspaceActorId = "local.actor";
    config.localWorkspaceLockTargetPath = manifest;
    config.localWorkspacePresenceLockReportPath = reportPath;

    std::ostringstream out;
    std::ostringstream err;
    const int exitCode = app.Run(config, out, err);

    EXPECT_EQ(exitCode, 0);
    EXPECT_NE(out.str().find("presence_lock.report="), std::string::npos);
    EXPECT_NE(out.str().find("presence_lock.status=Ready"),
              std::string::npos);
    EXPECT_TRUE(err.str().empty());
    EXPECT_EQ(ReadFile(manifest), manifestBefore);
    EXPECT_EQ(fs::last_write_time(manifest), manifestWriteTimeBefore);
    ASSERT_TRUE(fs::exists(reportPath));

    std::ifstream input(reportPath);
    const nlohmann::json report = nlohmann::json::parse(input);
    EXPECT_EQ(report["tool"], "Saga");
    EXPECT_EQ(report["command"], "local-workspace-presence-lock-smoke");
    EXPECT_EQ(report["verified"], false);
    EXPECT_EQ(report["status"], "Ready");
    EXPECT_EQ(report["workspace"]["workspaceId"], "builtin.basic");
    EXPECT_EQ(report["workspace"]["selector"], "builtin:basic");
    EXPECT_EQ(report["project"]["projectId"], projectId);
    EXPECT_EQ(report["actor"]["actorId"], "local.actor");
    EXPECT_EQ(report["presence"]["actorId"], "local.actor");
    EXPECT_EQ(report["presence"]["status"], "PresentLocal");
    EXPECT_EQ(report["presence"]["source"], "report-only");
    EXPECT_FALSE(report["presence"]["durable"].get<bool>());
    EXPECT_FALSE(report["presence"]["networked"].get<bool>());
    EXPECT_EQ(report["lock"]["targetArtifact"], fs::absolute(manifest).string());
    EXPECT_EQ(report["lock"]["lockMode"], "ReadOnlyEvaluation");
    EXPECT_EQ(report["lock"]["status"], "Ready");
    EXPECT_EQ(report["lock"]["conflictStatus"], "NotChecked");
    EXPECT_FALSE(report["lock"]["durable"].get<bool>());
    EXPECT_FALSE(report["lock"]["mutatesProject"].get<bool>());
    EXPECT_NE(report["lock"]["lockId"].get<std::string>().find(
                  "local-lock:builtin.basic:" + projectId + ":local.actor:"),
              std::string::npos);
    EXPECT_TRUE(JsonArrayContainsString(report["nonClaims"],
                                        "networked presence"));
    EXPECT_TRUE(JsonArrayContainsString(report["nonClaims"],
                                        "durable lock service"));
    EXPECT_TRUE(JsonArrayContainsString(report["nonClaims"],
                                        "enterprise access control"));
    EXPECT_TRUE(JsonArrayContainsString(report["nonClaims"],
                                        "collaboration server"));
    EXPECT_TRUE(JsonArrayContainsString(report["nonClaims"],
                                        "distribution readiness"));
}

TEST(SagaLocalReviewAuditSmokeTest,
     StarterArenaReportIsReadOnlyAndDoesNotClaimApprovalOrAuditService)
{
    const fs::path root =
        MakeTempDir("saga_local_review_audit_smoke_test");
    const fs::path reportPath =
        root / "starter_arena_review_audit_report.json";
    const fs::path manifest = SourceRoot() / "Samples" / "StarterArena" /
        "StarterArena.sagaproj";
    const std::string projectId = ProjectIdFromManifest(manifest);
    const std::string manifestBefore = ReadFile(manifest);
    const auto manifestWriteTimeBefore = fs::last_write_time(manifest);

    SagaProduct::SagaApp app;
    SagaAppConfig config;
    config.localWorkspaceReviewSmoke = true;
    config.workflowProjectPath = manifest;
    config.workspaceSelector = "builtin:basic";
    config.localWorkspaceActorId = "local.actor";
    config.localWorkspaceReviewTargetPath = manifest;
    config.localWorkspaceReviewComment =
        "Inspect StarterArena project metadata";
    config.localWorkspaceReviewReportPath = reportPath;

    std::ostringstream out;
    std::ostringstream err;
    const int exitCode = app.Run(config, out, err);

    EXPECT_EQ(exitCode, 0);
    EXPECT_NE(out.str().find("review.report="), std::string::npos);
    EXPECT_NE(out.str().find("review.status=Ready"), std::string::npos);
    EXPECT_TRUE(err.str().empty());
    EXPECT_EQ(ReadFile(manifest), manifestBefore);
    EXPECT_EQ(fs::last_write_time(manifest), manifestWriteTimeBefore);
    ASSERT_TRUE(fs::exists(reportPath));

    std::ifstream input(reportPath);
    const nlohmann::json report = nlohmann::json::parse(input);
    EXPECT_EQ(report["tool"], "Saga");
    EXPECT_EQ(report["command"], "local-workspace-review-smoke");
    EXPECT_EQ(report["verified"], false);
    EXPECT_EQ(report["status"], "Ready");
    EXPECT_EQ(report["workspace"]["workspaceId"], "builtin.basic");
    EXPECT_EQ(report["workspace"]["selector"], "builtin:basic");
    EXPECT_EQ(report["project"]["projectId"], projectId);
    EXPECT_EQ(report["actor"]["actorId"], "local.actor");
    EXPECT_EQ(report["review"]["targetArtifact"],
              fs::absolute(manifest).string());
    EXPECT_EQ(report["review"]["reviewMode"], "MetadataOnly");
    EXPECT_EQ(report["review"]["status"], "Ready");
    EXPECT_FALSE(report["review"]["durable"].get<bool>());
    EXPECT_FALSE(report["review"]["requiresApproval"].get<bool>());
    EXPECT_FALSE(report["review"]["mutatesProject"].get<bool>());
    EXPECT_EQ(report["comment"]["bodyEvaluation"],
              "Inspect StarterArena project metadata");
    EXPECT_EQ(report["comment"]["source"], "report-only");
    EXPECT_FALSE(report["comment"]["durable"].get<bool>());
    EXPECT_EQ(report["auditEvent"]["eventKind"],
              "LocalReviewMetadataRecorded");
    EXPECT_EQ(report["auditEvent"]["actorId"], "local.actor");
    EXPECT_EQ(report["auditEvent"]["source"], "report-only");
    EXPECT_FALSE(report["auditEvent"]["durable"].get<bool>());
    EXPECT_FALSE(report["auditEvent"]["tamperResistant"].get<bool>());
    EXPECT_NE(report["review"]["reviewId"].get<std::string>().find(
                  "local-review:builtin.basic:" + projectId + ":local.actor:"),
              std::string::npos);
    EXPECT_NE(report["comment"]["commentId"].get<std::string>().find(
                  "local-comment:builtin.basic:" + projectId + ":local.actor:"),
              std::string::npos);
    EXPECT_NE(report["auditEvent"]["eventId"].get<std::string>().find(
                  "local-audit:builtin.basic:" + projectId + ":local.actor:"),
              std::string::npos);
    EXPECT_TRUE(JsonArrayContainsString(report["nonClaims"],
                                        "approval workflow"));
    EXPECT_TRUE(JsonArrayContainsString(report["nonClaims"],
                                        "durable audit service"));
    EXPECT_TRUE(JsonArrayContainsString(report["nonClaims"],
                                        "tamper-resistant audit log"));
    EXPECT_TRUE(JsonArrayContainsString(report["nonClaims"],
                                        "collaboration server"));
    EXPECT_TRUE(JsonArrayContainsString(report["nonClaims"],
                                        "distribution readiness"));
}

TEST(SagaLocalRolePermissionSmokeTest,
     StarterArenaReportIsReadOnlyAndDoesNotClaimPermissionEnforcement)
{
    const fs::path root =
        MakeTempDir("saga_local_role_permission_smoke_test");
    const fs::path reportPath =
        root / "starter_arena_role_permission_report.json";
    const fs::path manifest = SourceRoot() / "Samples" / "StarterArena" /
        "StarterArena.sagaproj";
    const std::string projectId = ProjectIdFromManifest(manifest);
    const std::string manifestBefore = ReadFile(manifest);
    const auto manifestWriteTimeBefore = fs::last_write_time(manifest);

    SagaProduct::SagaApp app;
    SagaAppConfig config;
    config.localWorkspaceRoleSmoke = true;
    config.workflowProjectPath = manifest;
    config.workspaceSelector = "builtin:basic";
    config.localWorkspaceActorId = "local.actor";
    config.localWorkspaceRoleName = "local.reviewer";
    config.localWorkspacePermissionName = "inspect_project";
    config.localWorkspaceRoleReportPath = reportPath;

    std::ostringstream out;
    std::ostringstream err;
    const int exitCode = app.Run(config, out, err);

    EXPECT_EQ(exitCode, 0);
    EXPECT_NE(out.str().find("role.report="), std::string::npos);
    EXPECT_NE(out.str().find("role.status=Ready"), std::string::npos);
    EXPECT_TRUE(err.str().empty());
    EXPECT_EQ(ReadFile(manifest), manifestBefore);
    EXPECT_EQ(fs::last_write_time(manifest), manifestWriteTimeBefore);
    ASSERT_TRUE(fs::exists(reportPath));

    std::ifstream input(reportPath);
    const nlohmann::json report = nlohmann::json::parse(input);
    EXPECT_EQ(report["tool"], "Saga");
    EXPECT_EQ(report["command"], "local-workspace-role-smoke");
    EXPECT_EQ(report["verified"], false);
    EXPECT_EQ(report["status"], "Ready");
    EXPECT_EQ(report["workspace"]["workspaceId"], "builtin.basic");
    EXPECT_EQ(report["workspace"]["selector"], "builtin:basic");
    EXPECT_EQ(report["project"]["projectId"], projectId);
    EXPECT_EQ(report["actor"]["actorId"], "local.actor");
    EXPECT_EQ(report["role"]["actorId"], "local.actor");
    EXPECT_EQ(report["role"]["roleName"], "local.reviewer");
    EXPECT_EQ(report["role"]["source"], "report-only");
    EXPECT_FALSE(report["role"]["durable"].get<bool>());
    EXPECT_FALSE(report["role"]["enforced"].get<bool>());
    EXPECT_FALSE(report["role"]["networked"].get<bool>());
    EXPECT_EQ(report["permission"]["permissionName"], "inspect_project");
    EXPECT_EQ(report["permission"]["targetArtifact"],
              fs::absolute(manifest).lexically_normal().string());
    EXPECT_EQ(report["permission"]["status"], "Represented");
    EXPECT_FALSE(report["permission"]["enforced"].get<bool>());
    EXPECT_FALSE(report["permission"]["policyBacked"].get<bool>());
    EXPECT_FALSE(report["permission"]["mutatesProject"].get<bool>());
    EXPECT_NE(report["role"]["roleId"].get<std::string>().find(
                  "local-role:builtin.basic:" + projectId + ":local.actor:local.reviewer"),
              std::string::npos);
    EXPECT_NE(report["permission"]["permissionId"].get<std::string>().find(
                  "local-permission:builtin.basic:" + projectId + ":local.actor:inspect_project:"),
              std::string::npos);
    EXPECT_TRUE(JsonArrayContainsString(report["nonClaims"],
                                        "enterprise permissions"));
    EXPECT_TRUE(JsonArrayContainsString(report["nonClaims"],
                                        "real permission enforcement"));
    EXPECT_TRUE(JsonArrayContainsString(report["nonClaims"],
                                        "secure access control"));
    EXPECT_TRUE(JsonArrayContainsString(report["nonClaims"],
                                        "durable role service"));
    EXPECT_TRUE(JsonArrayContainsString(report["nonClaims"],
                                        "durable permission service"));
    EXPECT_TRUE(JsonArrayContainsString(report["nonClaims"],
                                        "networked authorization"));
    EXPECT_TRUE(JsonArrayContainsString(report["nonClaims"],
                                        "collaboration server"));
}

TEST(SagaLocalProjectSliceSmokeTest,
     StarterArenaReportIsReadOnlyAndDoesNotClaimVisibilityEnforcement)
{
    const fs::path root =
        MakeTempDir("saga_local_project_slice_smoke_test");
    const fs::path reportPath =
        root / "starter_arena_project_slice_report.json";
    const fs::path manifest = SourceRoot() / "Samples" / "StarterArena" /
        "StarterArena.sagaproj";
    const std::string projectId = ProjectIdFromManifest(manifest);
    const std::string manifestBefore = ReadFile(manifest);
    const auto manifestWriteTimeBefore = fs::last_write_time(manifest);

    SagaProduct::SagaApp app;
    SagaAppConfig config;
    config.localWorkspaceSliceSmoke = true;
    config.workflowProjectPath = manifest;
    config.workspaceSelector = "builtin:basic";
    config.localWorkspaceActorId = "local.actor";
    config.localWorkspaceSliceName = "starterarena.project_overview";
    config.localWorkspaceSliceTargetPath = manifest;
    config.localWorkspaceSliceReportPath = reportPath;

    std::ostringstream out;
    std::ostringstream err;
    const int exitCode = app.Run(config, out, err);

    EXPECT_EQ(exitCode, 0);
    EXPECT_NE(out.str().find("slice.report="), std::string::npos);
    EXPECT_NE(out.str().find("slice.status=Ready"), std::string::npos);
    EXPECT_TRUE(err.str().empty());
    EXPECT_EQ(ReadFile(manifest), manifestBefore);
    EXPECT_EQ(fs::last_write_time(manifest), manifestWriteTimeBefore);
    ASSERT_TRUE(fs::exists(reportPath));

    std::ifstream input(reportPath);
    const nlohmann::json report = nlohmann::json::parse(input);
    EXPECT_EQ(report["tool"], "Saga");
    EXPECT_EQ(report["command"], "local-workspace-slice-smoke");
    EXPECT_EQ(report["verified"], false);
    EXPECT_EQ(report["status"], "Ready");
    EXPECT_EQ(report["workspace"]["workspaceId"], "builtin.basic");
    EXPECT_EQ(report["workspace"]["selector"], "builtin:basic");
    EXPECT_EQ(report["project"]["projectId"], projectId);
    EXPECT_EQ(report["actor"]["actorId"], "local.actor");
    EXPECT_EQ(report["slice"]["displayName"],
              "starterarena.project_overview");
    EXPECT_EQ(report["slice"]["targetArtifact"],
              fs::absolute(manifest).lexically_normal().string());
    EXPECT_EQ(report["slice"]["sliceMode"], "MetadataOnly");
    EXPECT_EQ(report["slice"]["status"], "Represented");
    EXPECT_FALSE(report["slice"]["durable"].get<bool>());
    EXPECT_FALSE(report["slice"]["mutatesProject"].get<bool>());
    EXPECT_TRUE(report["visibility"]["visibleToActor"].get<bool>());
    EXPECT_EQ(report["visibility"]["visibilitySource"], "report-only");
    EXPECT_FALSE(report["visibility"]["permissionEnforced"].get<bool>());
    EXPECT_FALSE(report["visibility"]["policyBacked"].get<bool>());
    EXPECT_FALSE(report["visibility"]["networked"].get<bool>());
    EXPECT_NE(report["slice"]["sliceId"].get<std::string>().find(
                  "local-slice:builtin.basic:" + projectId + ":local.actor:starterarena.project_overview:"),
              std::string::npos);
    EXPECT_TRUE(JsonArrayContainsString(report["nonClaims"],
                                        "enterprise permissions"));
    EXPECT_TRUE(JsonArrayContainsString(report["nonClaims"],
                                        "secure access control"));
    EXPECT_TRUE(JsonArrayContainsString(report["nonClaims"],
                                        "networked authorization"));
    EXPECT_TRUE(JsonArrayContainsString(report["nonClaims"],
                                        "collaboration server"));
    EXPECT_TRUE(JsonArrayContainsString(report["nonClaims"],
                                        "distribution readiness"));
}

TEST(SagaLocalApprovalGateSmokeTest,
     StarterArenaReportIsReadOnlyAndDoesNotClaimPublishReadiness)
{
    const fs::path root =
        MakeTempDir("saga_local_approval_gate_smoke_test");
    const fs::path reportPath =
        root / "starter_arena_approval_gate_report.json";
    const fs::path manifest = SourceRoot() / "Samples" / "StarterArena" /
        "StarterArena.sagaproj";
    const std::string projectId = ProjectIdFromManifest(manifest);
    const std::string manifestBefore = ReadFile(manifest);
    const auto manifestWriteTimeBefore = fs::last_write_time(manifest);

    SagaProduct::SagaApp app;
    SagaAppConfig config;
    config.localWorkspaceApprovalGateSmoke = true;
    config.workflowProjectPath = manifest;
    config.workspaceSelector = "builtin:basic";
    config.localWorkspaceActorId = "local.actor";
    config.localWorkspaceRoleName = "local.reviewer";
    config.localWorkspaceGateTargetPath = manifest;
    config.localWorkspaceApprovalState = "approved-local-evaluation";
    config.localWorkspaceApprovalGateReportPath = reportPath;

    std::ostringstream out;
    std::ostringstream err;
    const int exitCode = app.Run(config, out, err);

    EXPECT_EQ(exitCode, 0);
    EXPECT_NE(out.str().find("approval_gate.report="), std::string::npos);
    EXPECT_NE(out.str().find("approval_gate.status=Ready"),
              std::string::npos);
    EXPECT_TRUE(err.str().empty());
    EXPECT_EQ(ReadFile(manifest), manifestBefore);
    EXPECT_EQ(fs::last_write_time(manifest), manifestWriteTimeBefore);
    ASSERT_TRUE(fs::exists(reportPath));

    std::ifstream input(reportPath);
    const nlohmann::json report = nlohmann::json::parse(input);
    EXPECT_EQ(report["tool"], "Saga");
    EXPECT_EQ(report["command"], "local-workspace-approval-gate-smoke");
    EXPECT_EQ(report["verified"], false);
    EXPECT_EQ(report["status"], "Ready");
    EXPECT_EQ(report["workspace"]["workspaceId"], "builtin.basic");
    EXPECT_EQ(report["workspace"]["selector"], "builtin:basic");
    EXPECT_EQ(report["project"]["projectId"], projectId);
    EXPECT_EQ(report["actor"]["actorId"], "local.actor");
    EXPECT_EQ(report["approval"]["actorId"], "local.actor");
    EXPECT_EQ(report["approval"]["roleName"], "local.reviewer");
    EXPECT_EQ(report["approval"]["targetArtifact"],
              fs::absolute(manifest).lexically_normal().string());
    EXPECT_EQ(report["approval"]["approvalState"], "ApprovedLocalEvaluation");
    EXPECT_EQ(report["approval"]["source"], "report-only");
    EXPECT_FALSE(report["approval"]["durable"].get<bool>());
    EXPECT_FALSE(report["approval"]["enforced"].get<bool>());
    EXPECT_FALSE(report["approval"]["requiresServer"].get<bool>());
    EXPECT_FALSE(report["approval"]["mutatesProject"].get<bool>());
    EXPECT_EQ(report["publishGate"]["targetArtifact"],
              fs::absolute(manifest).lexically_normal().string());
    EXPECT_EQ(report["publishGate"]["gateMode"], "MetadataOnly");
    EXPECT_EQ(report["publishGate"]["status"], "Blocked");
    EXPECT_EQ(report["publishGate"]["packagePreflightStatus"], "Blocked");
    EXPECT_FALSE(report["publishGate"]["distributionReady"].get<bool>());
    EXPECT_FALSE(report["publishGate"]["policyBacked"].get<bool>());
    EXPECT_FALSE(report["publishGate"]["enforced"].get<bool>());
    EXPECT_FALSE(report["publishGate"]["durable"].get<bool>());
    EXPECT_FALSE(report["publishGate"]["mutatesProject"].get<bool>());
    EXPECT_FALSE(report["readiness"]["canPublish"].get<bool>());
    EXPECT_NE(report["readiness"]["reason"].get<std::string>().find(
                  "Package and distribution readiness are not implemented"),
              std::string::npos);
    EXPECT_TRUE(JsonArrayContainsString(
        report["readiness"]["blockingLimitations"],
        "Package preflight is blocked in this metadata-only evaluation."));
    EXPECT_TRUE(JsonArrayContainsString(
        report["readiness"]["referencedReports"],
        "local-workspace-review-smoke"));
    EXPECT_TRUE(JsonArrayContainsString(
        report["readiness"]["referencedReports"],
        "workflow-smoke package_preflight reference"));
    EXPECT_NE(report["approval"]["approvalId"].get<std::string>().find(
                  "local-approval:builtin.basic:" + projectId + ":local.actor:local.reviewer:ApprovedLocalEvaluation:"),
              std::string::npos);
    EXPECT_NE(report["publishGate"]["gateId"].get<std::string>().find(
                  "local-approval-gate:builtin.basic:" + projectId + ":local.actor:"),
              std::string::npos);
    EXPECT_TRUE(JsonArrayContainsString(report["nonClaims"],
                                        "real approval workflow"));
    EXPECT_TRUE(JsonArrayContainsString(report["nonClaims"],
                                        "actual publish blocker"));
    EXPECT_TRUE(JsonArrayContainsString(report["nonClaims"],
                                        "package readiness"));
    EXPECT_TRUE(JsonArrayContainsString(report["nonClaims"],
                                        "distribution readiness"));
    EXPECT_TRUE(JsonArrayContainsString(report["nonClaims"],
                                        "enterprise policy engine"));
    EXPECT_TRUE(JsonArrayContainsString(report["nonClaims"],
                                        "secure access control"));
}

} // namespace
