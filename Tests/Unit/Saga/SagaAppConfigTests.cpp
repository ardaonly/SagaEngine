// SPDX-License-Identifier: MPL-2.0
#include "SagaProductTestSupport.hpp"

using namespace SagaProduct;
using namespace SagaProductTestSupport;

namespace
{
TEST(SagaAppConfigTest, DefaultConfigCanBeCreated)
{
    const char* argvRaw[] = { "Saga" };
    auto* argv = const_cast<char**>(argvRaw);

    const SagaConfigResult result = ParseSagaAppConfig(1, argv);

    ASSERT_TRUE(result.ok) << result.error;
    EXPECT_EQ(result.config.workspaceSelector, "builtin:basic");
    EXPECT_EQ(result.config.target, SagaProductTargetKind::Editor);
    EXPECT_TRUE(result.config.authoringMode.empty());
    EXPECT_FALSE(result.config.prepareOnly);
}

TEST(SagaAppConfigTest, ParsesCanonicalDesktopProjectAndAuthoringMode)
{
    const char* argvRaw[] = {
        "Saga", "Game.sagaproj", "--authoring-mode", "level3"};
    auto* argv = const_cast<char**>(argvRaw);

    const SagaConfigResult result = ParseSagaAppConfig(4, argv);

    ASSERT_TRUE(result.ok) << result.error;
    EXPECT_EQ(result.config.workflowProjectPath, fs::path("Game.sagaproj"));
    EXPECT_EQ(result.config.authoringMode, "level3");
}

TEST(SagaAppConfigTest, RejectsUnknownAuthoringMode)
{
    const char* argvRaw[] = {"Saga", "--authoring-mode", "expert"};
    auto* argv = const_cast<char**>(argvRaw);

    const SagaConfigResult result = ParseSagaAppConfig(3, argv);

    EXPECT_FALSE(result.ok);
    EXPECT_NE(result.error.find("unknown authoring mode"), std::string::npos);
}

TEST(SagaAppConfigTest, LegacyProfileMapsOnlyWhenNoExplicitAuthoringMode)
{
    const char* legacyRaw[] = {"Saga", "--profile", "advanced_pipeline"};
    auto* legacy = const_cast<char**>(legacyRaw);
    const SagaConfigResult migrated = ParseSagaAppConfig(3, legacy);
    ASSERT_TRUE(migrated.ok) << migrated.error;
    EXPECT_EQ(migrated.config.authoringMode, "level3");

    const char* explicitRaw[] = {
        "Saga", "--authoring-mode", "level1", "--profile", "advanced_pipeline"};
    auto* explicitArgs = const_cast<char**>(explicitRaw);
    const SagaConfigResult explicitResult =
        ParseSagaAppConfig(5, explicitArgs);
    ASSERT_TRUE(explicitResult.ok) << explicitResult.error;
    EXPECT_EQ(explicitResult.config.authoringMode, "level1");
}

TEST(SagaAppConfigTest, LauncherDistributionReportOverrideIsReadOnlyInput)
{
    const char* argvRaw[] = {
        "Saga", "--launcher-distribution-report", "/tmp/distribution-report.json"};
    auto* argv = const_cast<char**>(argvRaw);

    const SagaConfigResult result = ParseSagaAppConfig(3, argv);

    ASSERT_TRUE(result.ok) << result.error;
    EXPECT_EQ(result.config.launcherDistributionReportPath,
              fs::path("/tmp/distribution-report.json"));
    EXPECT_FALSE(result.config.prepareOnly);
}

TEST(SagaProcessServiceTest, AllowsOnlyTypedExecutableIdentities)
{
    EXPECT_TRUE(SagaProcessService::IsExecutableAllowed(
        SagaProcessTargetId::Runtime, "/opt/saga/bin/SagaRuntime"));
    EXPECT_TRUE(SagaProcessService::IsExecutableAllowed(
        SagaProcessTargetId::Forge, "forge"));
    EXPECT_TRUE(SagaProcessService::IsExecutableAllowed(
        SagaProcessTargetId::SagaScript, "sagascript"));
    EXPECT_FALSE(SagaProcessService::IsExecutableAllowed(
        SagaProcessTargetId::Runtime, "/bin/sh"));
    EXPECT_FALSE(SagaProcessService::IsExecutableAllowed(
        SagaProcessTargetId::Runtime, "/opt/saga/bin/SagaEditor"));
}

TEST(SagaProcessServiceTest, EnvironmentOverridesUseExactAllowlist)
{
    EXPECT_TRUE(SagaProcessService::IsEnvironmentKeyAllowed(
        "SAGASCRIPT_RUNTIME_BRIDGE_ASSEMBLY"));
    EXPECT_TRUE(SagaProcessService::IsEnvironmentKeyAllowed(
        "QT_QPA_PLATFORM"));
    EXPECT_FALSE(SagaProcessService::IsEnvironmentKeyAllowed("PATH"));
    EXPECT_FALSE(SagaProcessService::IsEnvironmentKeyAllowed("LD_PRELOAD"));
}

TEST(SagaAppConfigTest, QtLaunchArgumentsArePassedThrough)
{
    const char* argvRaw[] = {
        "Saga",
        "--platform",
        "offscreen",
        "--style=Fusion",
    };
    auto* argv = const_cast<char**>(argvRaw);

    const SagaConfigResult result = ParseSagaAppConfig(4, argv);

    ASSERT_TRUE(result.ok) << result.error;
    EXPECT_EQ(result.config.target, SagaProductTargetKind::Editor);
    EXPECT_FALSE(result.config.prepareOnly);
}

TEST(SagaAppConfigTest, PackageManifestArgumentIsParsed)
{
    const char* argvRaw[] = {
        "Saga",
        "--target",
        "runtime",
        "--package-manifest",
        "Packages/dev-client/package.json",
    };
    auto* argv = const_cast<char**>(argvRaw);

    const SagaConfigResult result = ParseSagaAppConfig(5, argv);

    ASSERT_TRUE(result.ok) << result.error;
    ASSERT_TRUE(result.config.packageManifestPath.has_value());
    EXPECT_EQ(*result.config.packageManifestPath,
              fs::path("Packages/dev-client/package.json"));
    EXPECT_EQ(result.config.target, SagaProductTargetKind::Runtime);
}

TEST(SagaAppConfigTest, SagaScriptValidationArgumentsAreParsed)
{
    const char* argvRaw[] = {
        "Saga",
        "--validate-sagascript",
        "/tmp/project/Project.sagaproj",
        "--forge",
        "/tools/forge",
        "--sagascript-tool",
        "/tools/sagascript",
    };
    auto* argv = const_cast<char**>(argvRaw);

    const SagaConfigResult result = ParseSagaAppConfig(7, argv);

    ASSERT_TRUE(result.ok) << result.error;
    EXPECT_TRUE(result.config.validateSagaScript);
    EXPECT_EQ(result.config.sagaScriptProjectManifest,
              fs::path("/tmp/project/Project.sagaproj"));
    EXPECT_EQ(result.config.forgeExecutable, fs::path("/tools/forge"));
    EXPECT_EQ(result.config.sagaScriptExecutable, fs::path("/tools/sagascript"));
}

TEST(SagaAppConfigTest, PackageCommandsRedirectToSagapack)
{
    const char* argvRaw[] = {
        "Saga",
        "--publish-check",
        "/tmp/project/Project.sagaproj",
    };
    auto* argv = const_cast<char**>(argvRaw);

    const SagaConfigResult result = ParseSagaAppConfig(3, argv);

    EXPECT_FALSE(result.ok);
    EXPECT_NE(result.error.find("sagapack stage"), std::string::npos);
    EXPECT_NE(result.error.find("sagapack publish-check"), std::string::npos);
}

TEST(SagaAppConfigTest, WorkflowSmokeArgumentsAreParsed)
{
    const char* argvRaw[] = {
        "Saga",
        "--workflow-smoke",
        "--project",
        "Samples/StarterArena/StarterArena.sagaproj",
        "--profile",
        "project_readiness",
        "--workflow-report-out",
        "/tmp/starter_arena_product_shell_workflow_report.json",
    };
    auto* argv = const_cast<char**>(argvRaw);

    const SagaConfigResult result = ParseSagaAppConfig(8, argv);

    ASSERT_TRUE(result.ok) << result.error;
    EXPECT_TRUE(result.config.workflowSmoke);
    EXPECT_EQ(result.config.workflowProjectPath,
              fs::path("Samples/StarterArena/StarterArena.sagaproj"));
    EXPECT_EQ(result.config.workflowProfile, "project_readiness");
    EXPECT_EQ(result.config.workflowReportPath,
              fs::path("/tmp/starter_arena_product_shell_workflow_report.json"));
}

TEST(SagaAppConfigTest, LocalWorkspaceTransactionSmokeArgumentsAreParsed)
{
    const char* argvRaw[] = {
        "Saga",
        "--local-workspace-transaction-smoke",
        "--project",
        "Samples/StarterArena/StarterArena.sagaproj",
        "--workspace",
        "builtin:basic",
        "--actor",
        "local.actor",
        "--operation",
        "InspectProject",
        "--transaction-report-out",
        "/tmp/starter_arena_local_workspace_transaction_report.json",
    };
    auto* argv = const_cast<char**>(argvRaw);

    const SagaConfigResult result = ParseSagaAppConfig(12, argv);

    ASSERT_TRUE(result.ok) << result.error;
    EXPECT_TRUE(result.config.localWorkspaceTransactionSmoke);
    EXPECT_EQ(result.config.workflowProjectPath,
              fs::path("Samples/StarterArena/StarterArena.sagaproj"));
    EXPECT_EQ(result.config.workspaceSelector, "builtin:basic");
    EXPECT_EQ(result.config.localWorkspaceActorId, "local.actor");
    EXPECT_EQ(result.config.localWorkspaceOperationKind, "InspectProject");
    EXPECT_EQ(
        result.config.localWorkspaceTransactionReportPath,
        fs::path("/tmp/starter_arena_local_workspace_transaction_report.json"));
}

TEST(SagaAppConfigTest, LocalWorkspacePresenceLockSmokeArgumentsAreParsed)
{
    const char* argvRaw[] = {
        "Saga",
        "--local-workspace-presence-lock-smoke",
        "--project",
        "Samples/StarterArena/StarterArena.sagaproj",
        "--workspace",
        "builtin:basic",
        "--actor",
        "local.actor",
        "--lock-target",
        "Samples/StarterArena/StarterArena.sagaproj",
        "--presence-lock-report-out",
        "/tmp/starter_arena_presence_lock_report.json",
    };
    auto* argv = const_cast<char**>(argvRaw);

    const SagaConfigResult result = ParseSagaAppConfig(12, argv);

    ASSERT_TRUE(result.ok) << result.error;
    EXPECT_TRUE(result.config.localWorkspacePresenceLockSmoke);
    EXPECT_EQ(result.config.workflowProjectPath,
              fs::path("Samples/StarterArena/StarterArena.sagaproj"));
    EXPECT_EQ(result.config.workspaceSelector, "builtin:basic");
    EXPECT_EQ(result.config.localWorkspaceActorId, "local.actor");
    EXPECT_EQ(result.config.localWorkspaceLockTargetPath,
              fs::path("Samples/StarterArena/StarterArena.sagaproj"));
    EXPECT_EQ(result.config.localWorkspacePresenceLockReportPath,
              fs::path("/tmp/starter_arena_presence_lock_report.json"));
}

TEST(SagaAppConfigTest, LocalWorkspaceReviewSmokeArgumentsAreParsed)
{
    const char* argvRaw[] = {
        "Saga",
        "--local-workspace-review-smoke",
        "--project",
        "Samples/StarterArena/StarterArena.sagaproj",
        "--workspace",
        "builtin:basic",
        "--actor",
        "local.actor",
        "--review-target",
        "Samples/StarterArena/StarterArena.sagaproj",
        "--comment",
        "Inspect StarterArena project metadata",
        "--review-report-out",
        "/tmp/starter_arena_review_audit_report.json",
    };
    auto* argv = const_cast<char**>(argvRaw);

    const SagaConfigResult result = ParseSagaAppConfig(14, argv);

    ASSERT_TRUE(result.ok) << result.error;
    EXPECT_TRUE(result.config.localWorkspaceReviewSmoke);
    EXPECT_EQ(result.config.workflowProjectPath,
              fs::path("Samples/StarterArena/StarterArena.sagaproj"));
    EXPECT_EQ(result.config.workspaceSelector, "builtin:basic");
    EXPECT_EQ(result.config.localWorkspaceActorId, "local.actor");
    EXPECT_EQ(result.config.localWorkspaceReviewTargetPath,
              fs::path("Samples/StarterArena/StarterArena.sagaproj"));
    EXPECT_EQ(result.config.localWorkspaceReviewComment,
              "Inspect StarterArena project metadata");
    EXPECT_EQ(result.config.localWorkspaceReviewReportPath,
              fs::path("/tmp/starter_arena_review_audit_report.json"));
}

TEST(SagaAppConfigTest, LocalWorkspaceRoleSmokeArgumentsAreParsed)
{
    const char* argvRaw[] = {
        "Saga",
        "--local-workspace-role-smoke",
        "--project",
        "Samples/StarterArena/StarterArena.sagaproj",
        "--workspace",
        "builtin:basic",
        "--actor",
        "local.actor",
        "--role",
        "local.reviewer",
        "--permission",
        "inspect_project",
        "--role-report-out",
        "/tmp/starter_arena_role_permission_report.json",
    };
    auto* argv = const_cast<char**>(argvRaw);

    const SagaConfigResult result = ParseSagaAppConfig(14, argv);

    ASSERT_TRUE(result.ok) << result.error;
    EXPECT_TRUE(result.config.localWorkspaceRoleSmoke);
    EXPECT_EQ(result.config.workflowProjectPath,
              fs::path("Samples/StarterArena/StarterArena.sagaproj"));
    EXPECT_EQ(result.config.workspaceSelector, "builtin:basic");
    EXPECT_EQ(result.config.localWorkspaceActorId, "local.actor");
    EXPECT_EQ(result.config.localWorkspaceRoleName, "local.reviewer");
    EXPECT_EQ(result.config.localWorkspacePermissionName, "inspect_project");
    EXPECT_EQ(result.config.localWorkspaceRoleReportPath,
              fs::path("/tmp/starter_arena_role_permission_report.json"));
}

TEST(SagaAppConfigTest, LocalWorkspaceSliceSmokeArgumentsAreParsed)
{
    const char* argvRaw[] = {
        "Saga",
        "--local-workspace-slice-smoke",
        "--project",
        "Samples/StarterArena/StarterArena.sagaproj",
        "--workspace",
        "builtin:basic",
        "--actor",
        "local.actor",
        "--slice",
        "starterarena.project_overview",
        "--slice-target",
        "Samples/StarterArena/StarterArena.sagaproj",
        "--slice-report-out",
        "/tmp/starter_arena_project_slice_report.json",
    };
    auto* argv = const_cast<char**>(argvRaw);

    const SagaConfigResult result = ParseSagaAppConfig(14, argv);

    ASSERT_TRUE(result.ok) << result.error;
    EXPECT_TRUE(result.config.localWorkspaceSliceSmoke);
    EXPECT_EQ(result.config.workflowProjectPath,
              fs::path("Samples/StarterArena/StarterArena.sagaproj"));
    EXPECT_EQ(result.config.workspaceSelector, "builtin:basic");
    EXPECT_EQ(result.config.localWorkspaceActorId, "local.actor");
    EXPECT_EQ(result.config.localWorkspaceSliceName,
              "starterarena.project_overview");
    EXPECT_EQ(result.config.localWorkspaceSliceTargetPath,
              fs::path("Samples/StarterArena/StarterArena.sagaproj"));
    EXPECT_EQ(result.config.localWorkspaceSliceReportPath,
              fs::path("/tmp/starter_arena_project_slice_report.json"));
}

TEST(SagaAppConfigTest, LocalWorkspaceApprovalGateSmokeArgumentsAreParsed)
{
    const char* argvRaw[] = {
        "Saga",
        "--local-workspace-approval-gate-smoke",
        "--project",
        "Samples/StarterArena/StarterArena.sagaproj",
        "--workspace",
        "builtin:basic",
        "--actor",
        "local.actor",
        "--role",
        "local.reviewer",
        "--gate-target",
        "Samples/StarterArena/StarterArena.sagaproj",
        "--approval-state",
        "approved-local-evaluation",
        "--approval-gate-report-out",
        "/tmp/starter_arena_approval_gate_report.json",
    };
    auto* argv = const_cast<char**>(argvRaw);

    const SagaConfigResult result = ParseSagaAppConfig(16, argv);

    ASSERT_TRUE(result.ok) << result.error;
    EXPECT_TRUE(result.config.localWorkspaceApprovalGateSmoke);
    EXPECT_EQ(result.config.workflowProjectPath,
              fs::path("Samples/StarterArena/StarterArena.sagaproj"));
    EXPECT_EQ(result.config.workspaceSelector, "builtin:basic");
    EXPECT_EQ(result.config.localWorkspaceActorId, "local.actor");
    EXPECT_EQ(result.config.localWorkspaceRoleName, "local.reviewer");
    EXPECT_EQ(result.config.localWorkspaceGateTargetPath,
              fs::path("Samples/StarterArena/StarterArena.sagaproj"));
    EXPECT_EQ(result.config.localWorkspaceApprovalState,
              "approved-local-evaluation");
    EXPECT_EQ(result.config.localWorkspaceApprovalGateReportPath,
              fs::path("/tmp/starter_arena_approval_gate_report.json"));
}

TEST(SagaAppConfigTest, MissingPackageManifestValueFailsDeterministically)
{
    const char* argvRaw[] = {
        "Saga",
        "--package-manifest",
    };
    auto* argv = const_cast<char**>(argvRaw);

    const SagaConfigResult result = ParseSagaAppConfig(2, argv);

    EXPECT_FALSE(result.ok);
    EXPECT_EQ(result.error, "Saga: --package-manifest requires a path");
}

} // namespace
