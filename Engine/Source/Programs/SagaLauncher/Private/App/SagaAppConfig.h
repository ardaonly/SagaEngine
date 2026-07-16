/// @file SagaAppConfig.h
/// @brief Product startup configuration and distribution metadata loading.

#pragma once

#include "Projects/SagaSessionModel.h"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace SagaProduct
{

/// Version and binary inventory published by a staged Saga distribution.
struct SagaProductInfo
{
    std::string              productName = "SAGA";
    std::string              distributionVersion;
    std::string              buildVersion;
    std::string              gitCommit;
    std::string              platform;
    std::vector<std::string> binaries;
};

/// Parsed command line for the Saga product entry point.
struct SagaAppConfig
{
    std::filesystem::path executablePath;
    std::filesystem::path versionInfoPath;
    std::filesystem::path launcherDistributionReportPath;
    std::filesystem::path builtInWorkspaceRoot;
    std::filesystem::path sagaScriptProjectManifest;
    std::filesystem::path workflowProjectPath;
    std::filesystem::path workflowReportPath;
    std::filesystem::path firstPlayableOutputDirectory;
    std::filesystem::path firstPlayableSummaryPath;
    std::filesystem::path firstPlayableKeyboardReportPath;
    std::filesystem::path runtimeExecutable;
    std::filesystem::path runtimeBridgeAssembly;
    std::filesystem::path localWorkspaceTransactionReportPath;
    std::filesystem::path localWorkspacePresenceLockReportPath;
    std::filesystem::path localWorkspaceLockTargetPath;
    std::filesystem::path localWorkspaceReviewReportPath;
    std::filesystem::path localWorkspaceReviewTargetPath;
    std::filesystem::path localWorkspaceRoleReportPath;
    std::filesystem::path localWorkspaceSliceReportPath;
    std::filesystem::path localWorkspaceSliceTargetPath;
    std::filesystem::path localWorkspaceApprovalGateReportPath;
    std::filesystem::path localWorkspaceGateTargetPath;
    std::filesystem::path forgeExecutable = "forge";
    std::filesystem::path sagaScriptExecutable = "sagascript";
    std::optional<std::filesystem::path> packageManifestPath;
    std::string           workspaceSelector = "builtin:basic";
    std::string           workflowProfile = "project_readiness";
    std::string           localWorkspaceActorId = "local.actor";
    std::string           localWorkspaceOperationKind = "InspectProject";
    std::string           localWorkspaceReviewComment =
        "Inspect project metadata";
    std::string           localWorkspaceRoleName;
    std::string           localWorkspacePermissionName;
    std::string           localWorkspaceSliceName;
    std::string           localWorkspaceApprovalState =
        "approved-local-evaluation";
    SagaProductTargetKind target = SagaProductTargetKind::Editor;
    bool                  validateSagaScript = false;
    bool                  workflowSmoke = false;
    bool                  firstPlayableCheck = false;
    bool                  firstPlayableHumanCapture = false;
    int                   firstPlayableTimeoutMs = 30000;
    int                   firstPlayableHumanTimeoutMs = 30000;
    int                   firstPlayableHumanFrames = 600;
    bool                  localWorkspaceTransactionSmoke = false;
    bool                  localWorkspacePresenceLockSmoke = false;
    bool                  localWorkspaceReviewSmoke = false;
    bool                  localWorkspaceRoleSmoke = false;
    bool                  localWorkspaceSliceSmoke = false;
    bool                  localWorkspaceApprovalGateSmoke = false;
    bool                  prepareOnly = false;
    bool                  showHelp = false;
};

/// Outcome for config parsing and metadata loading.
struct SagaConfigResult
{
    bool          ok = false;
    SagaAppConfig config;
    std::string   error;
};

[[nodiscard]] SagaConfigResult ParseSagaAppConfig(int argc, char* argv[]);
[[nodiscard]] SagaProductInfo LoadProductInfo(const SagaAppConfig& config,
                                              std::string& outError);
[[nodiscard]] SagaProductInfo MakeBuiltProductInfo();
[[nodiscard]] std::string SagaUsageText();

} // namespace SagaProduct
