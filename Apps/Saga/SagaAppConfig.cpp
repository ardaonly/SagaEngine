/// @file SagaAppConfig.cpp
/// @brief Product startup configuration and distribution metadata loading.

#include "SagaAppConfig.h"

#include <nlohmann/json.hpp>

#include <fstream>
#include <sstream>

#ifndef SAGA_PRODUCT_VERSION
#define SAGA_PRODUCT_VERSION "0.0.0"
#endif

#ifndef SAGA_PRODUCT_GIT_COMMIT
#define SAGA_PRODUCT_GIT_COMMIT "unknown"
#endif

#ifndef SAGA_PRODUCT_PLATFORM
#define SAGA_PRODUCT_PLATFORM "unknown"
#endif

#ifndef SAGA_BUILTIN_BASIC_WORKSPACE_ROOT
#define SAGA_BUILTIN_BASIC_WORKSPACE_ROOT ""
#endif

namespace SagaProduct
{
namespace
{

[[nodiscard]] bool HasValue(int index, int argc) noexcept
{
    return index + 1 < argc;
}

[[nodiscard]] bool IsQtOptionWithValue(const std::string& arg) noexcept
{
    return arg == "--platform" || arg == "-platform" ||
        arg == "--style" || arg == "-style" ||
        arg == "--stylesheet" || arg == "-stylesheet";
}

[[nodiscard]] bool IsQtOptionWithInlineValue(const std::string& arg) noexcept
{
    return arg.rfind("--platform=", 0) == 0 ||
        arg.rfind("-platform=", 0) == 0 ||
        arg.rfind("--style=", 0) == 0 ||
        arg.rfind("-style=", 0) == 0 ||
        arg.rfind("--stylesheet=", 0) == 0 ||
        arg.rfind("-stylesheet=", 0) == 0;
}

[[nodiscard]] std::filesystem::path ResolveDefaultVersionInfo(
    const std::filesystem::path& executablePath)
{
    if (executablePath.empty())
    {
        return {};
    }
    return executablePath.parent_path().parent_path() / "version.json";
}

[[nodiscard]] std::filesystem::path ResolveDefaultBuiltInWorkspace(
    const std::filesystem::path& executablePath)
{
    if (!executablePath.empty())
    {
        const auto installed =
            executablePath.parent_path().parent_path() / "config" /
            "workspaces" / "basic";
        if (std::filesystem::exists(installed))
        {
            return installed;
        }
    }
    return SAGA_BUILTIN_BASIC_WORKSPACE_ROOT;
}

[[nodiscard]] std::vector<std::string> ReadStringArray(const nlohmann::json& json,
                                                       const char* key)
{
    std::vector<std::string> values;
    if (!json.contains(key) || !json[key].is_array())
    {
        return values;
    }
    for (const auto& item : json[key])
    {
        if (item.is_string())
        {
            values.push_back(item.get<std::string>());
        }
    }
    return values;
}

[[nodiscard]] std::string ReadString(const nlohmann::json& json,
                                     const char* key,
                                     std::string fallback)
{
    if (json.contains(key) && json[key].is_string())
    {
        return json[key].get<std::string>();
    }
    return fallback;
}

} // namespace

std::string SagaUsageText()
{
    return
        "Usage: Saga [options]\n"
        "  --workspace <path|builtin:basic>  Product workspace root\n"
        "  --target <editor|runtime|server>  Product role to prepare (server execution unsupported)\n"
        "  --package-manifest <path>         Startup package manifest for runtime targets\n"
        "  --validate-sagascript <project>   Run SagaScript project validation through Forge gate\n"
        "  --stage-packages <project>        Generate client/server package manifests\n"
        "  --package-profile <name>          Package staging profile\n"
        "  --target-platform <name>          Package target platform token\n"
        "  --runtime-compatibility <version> Package runtime compatibility token\n"
        "  --package-report <path>           Package staging report output path\n"
        "  --publish-check <project>         Run product publish readiness check\n"
        "  --publish-profile <name>          Publish readiness profile\n"
        "  --publish-report <path>           Publish readiness report output path\n"
        "  --publish-diagnostics <key=path>  Include opaque diagnostics report in publish readiness\n"
        "  --workflow-smoke                  Emit Product Shell workflow smoke report\n"
        "  --project <path>                  Project manifest for workflow smoke\n"
        "  --profile <id>                    Workflow smoke profile/view preset id\n"
        "  --workflow-report-out <path>      Workflow smoke report output path\n"
        "  --first-playable-check            Run StarterArena Product Shell evidence workflow\n"
        "  --first-playable-human-capture    Capture/import real keyboard evidence, then run the gate\n"
        "  --first-playable-output <dir>     Generated evidence directory (outside project source)\n"
        "  --first-playable-summary-out <path> Consolidated JSON summary path\n"
        "  --first-playable-keyboard-report <path> Optional real-keyboard report to validate\n"
        "  --runtime-executable <path>       SagaRuntime executable override\n"
        "  --runtime-bridge-assembly <path> Managed SagaScript runtime bridge assembly\n"
        "  --first-playable-timeout-ms <ms> Per-process timeout (positive integer)\n"
        "  --first-playable-human-timeout-ms <ms> Human capture timeout (default 30000)\n"
        "  --first-playable-human-frames <n> Human capture frame bound (default 600)\n"
        "  --local-workspace-transaction-smoke Emit local workspace transaction report\n"
        "  --local-workspace-presence-lock-smoke Emit local presence/lock metadata report\n"
        "  --local-workspace-review-smoke Emit local review/comment/audit metadata report\n"
        "  --local-workspace-role-smoke Emit local role/permission metadata report\n"
        "  --local-workspace-slice-smoke Emit local project slice visibility metadata report\n"
        "  --local-workspace-approval-gate-smoke Emit local approval/publish gate metadata report\n"
        "  --actor <id>                      Local workspace transaction actor id\n"
        "  --operation <kind>                Local workspace transaction operation kind\n"
        "  --transaction-report-out <path>   Local workspace transaction report output path\n"
        "  --lock-target <path>              Local report lock target artifact\n"
        "  --presence-lock-report-out <path> Local presence/lock report output path\n"
        "  --review-target <path>            Local report review target artifact\n"
        "  --comment <text>                  Local review/comment metadata text\n"
        "  --review-report-out <path>        Local review/audit report output path\n"
        "  --role <name>                     Local report role label\n"
        "  --permission <name>               Local report permission label\n"
        "  --role-report-out <path>          Local role/permission report output path\n"
        "  --slice <name>                    Local project slice label\n"
        "  --slice-target <path>             Local project slice target artifact\n"
        "  --slice-report-out <path>         Local project slice report output path\n"
        "  --gate-target <path>              Local approval gate target artifact\n"
        "  --approval-state <state>          Local approval preview state\n"
        "  --approval-gate-report-out <path> Local approval gate report output path\n"
        "  --forge <path>                    Forge executable for SagaScript validation\n"
        "  --sagascript-tool <path>          SagaScript executable for SagaScript validation\n"
        "  --version-info <path>             Distribution version.json path\n"
        "  --launcher-distribution-report <path> Read-only launcher distribution report override\n"
        "  --prepare-only                    Dev diagnostic: validate and print target prep\n"
        "  --help                            Show this help\n";
}

SagaConfigResult ParseSagaAppConfig(int argc, char* argv[])
{
    SagaConfigResult result;
    result.ok = true;

    if (argc > 0 && argv != nullptr && argv[0] != nullptr)
    {
        result.config.executablePath =
            std::filesystem::absolute(std::filesystem::path(argv[0]));
    }
    result.config.versionInfoPath =
        ResolveDefaultVersionInfo(result.config.executablePath);
    result.config.builtInWorkspaceRoot =
        ResolveDefaultBuiltInWorkspace(result.config.executablePath);

    for (int i = 1; i < argc; ++i)
    {
        const std::string arg = argv[i] ? argv[i] : "";
        if (arg == "--help" || arg == "-h" || arg == "--?")
        {
            result.config.showHelp = true;
            return result;
        }
        if (arg == "--workspace")
        {
            if (!HasValue(i, argc))
            {
                result.ok = false;
                result.error = "Saga: --workspace requires a path or builtin:basic";
                return result;
            }
            result.config.workspaceSelector = argv[++i];
        }
        else if (arg == "--target")
        {
            if (!HasValue(i, argc))
            {
                result.ok = false;
                result.error = "Saga: --target requires editor, runtime, or server";
                return result;
            }
            SagaProductTargetKind parsed = SagaProductTargetKind::Editor;
            if (!ParseTargetKind(argv[++i], parsed))
            {
                result.ok = false;
                result.error = "Saga: unknown target '" + std::string(argv[i]) + "'";
                return result;
            }
            result.config.target = parsed;
        }
        else if (arg == "--version-info")
        {
            if (!HasValue(i, argc))
            {
                result.ok = false;
                result.error = "Saga: --version-info requires a path";
                return result;
            }
            result.config.versionInfoPath = argv[++i];
        }
        else if (arg == "--launcher-distribution-report")
        {
            if (!HasValue(i, argc))
            {
                result.ok = false;
                result.error =
                    "Saga: --launcher-distribution-report requires a path";
                return result;
            }
            result.config.launcherDistributionReportPath = argv[++i];
        }
        else if (arg == "--package-manifest")
        {
            if (!HasValue(i, argc))
            {
                result.ok = false;
                result.error = "Saga: --package-manifest requires a path";
                return result;
            }
            result.config.packageManifestPath = std::filesystem::path(argv[++i]);
        }
        else if (arg == "--validate-sagascript")
        {
            if (!HasValue(i, argc))
            {
                result.ok = false;
                result.error = "Saga: --validate-sagascript requires a project root";
                return result;
            }
            result.config.validateSagaScript = true;
            result.config.sagaScriptProjectRoot =
                std::filesystem::path(argv[++i]);
        }
        else if (arg == "--stage-packages")
        {
            if (!HasValue(i, argc))
            {
                result.ok = false;
                result.error = "Saga: --stage-packages requires a project root";
                return result;
            }
            result.config.stagePackages = true;
            result.config.packageStageProjectRoot =
                std::filesystem::path(argv[++i]);
        }
        else if (arg == "--package-profile")
        {
            if (!HasValue(i, argc))
            {
                result.ok = false;
                result.error = "Saga: --package-profile requires a name";
                return result;
            }
            result.config.packageProfile = argv[++i];
        }
        else if (arg == "--target-platform")
        {
            if (!HasValue(i, argc))
            {
                result.ok = false;
                result.error = "Saga: --target-platform requires a name";
                return result;
            }
            result.config.targetPlatform = argv[++i];
        }
        else if (arg == "--runtime-compatibility")
        {
            if (!HasValue(i, argc))
            {
                result.ok = false;
                result.error =
                    "Saga: --runtime-compatibility requires a version";
                return result;
            }
            result.config.runtimeCompatibilityVersion = argv[++i];
        }
        else if (arg == "--package-report")
        {
            if (!HasValue(i, argc))
            {
                result.ok = false;
                result.error = "Saga: --package-report requires a path";
                return result;
            }
            result.config.packageStageReportPath =
                std::filesystem::path(argv[++i]);
        }
        else if (arg == "--publish-check")
        {
            if (!HasValue(i, argc))
            {
                result.ok = false;
                result.error = "Saga: --publish-check requires a project root";
                return result;
            }
            result.config.publishCheck = true;
            result.config.publishProjectRoot = std::filesystem::path(argv[++i]);
        }
        else if (arg == "--publish-profile")
        {
            if (!HasValue(i, argc))
            {
                result.ok = false;
                result.error = "Saga: --publish-profile requires a name";
                return result;
            }
            result.config.publishProfile = argv[++i];
        }
        else if (arg == "--publish-report")
        {
            if (!HasValue(i, argc))
            {
                result.ok = false;
                result.error = "Saga: --publish-report requires a path";
                return result;
            }
            result.config.publishReportPath = std::filesystem::path(argv[++i]);
        }
        else if (arg == "--publish-diagnostics")
        {
            if (!HasValue(i, argc))
            {
                result.ok = false;
                result.error = "Saga: --publish-diagnostics requires key=path";
                return result;
            }
            result.config.publishDiagnostics.push_back(argv[++i]);
        }
        else if (arg == "--workflow-smoke")
        {
            result.config.workflowSmoke = true;
        }
        else if (arg == "--first-playable-check")
        {
            result.config.firstPlayableCheck = true;
        }
        else if (arg == "--first-playable-human-capture")
        {
            result.config.firstPlayableHumanCapture = true;
        }
        else if (arg == "--project")
        {
            if (!HasValue(i, argc))
            {
                result.ok = false;
                result.error = "Saga: --project requires a path";
                return result;
            }
            result.config.workflowProjectPath = std::filesystem::path(argv[++i]);
        }
        else if (arg == "--profile")
        {
            if (!HasValue(i, argc))
            {
                result.ok = false;
                result.error = "Saga: --profile requires a profile id";
                return result;
            }
            result.config.workflowProfile = argv[++i];
        }
        else if (arg == "--workflow-report-out")
        {
            if (!HasValue(i, argc))
            {
                result.ok = false;
                result.error = "Saga: --workflow-report-out requires a path";
                return result;
            }
            result.config.workflowReportPath = std::filesystem::path(argv[++i]);
        }
        else if (arg == "--first-playable-output")
        {
            if (!HasValue(i, argc))
            {
                result.ok = false;
                result.error = "Saga: --first-playable-output requires a directory";
                return result;
            }
            result.config.firstPlayableOutputDirectory = argv[++i];
        }
        else if (arg == "--first-playable-summary-out")
        {
            if (!HasValue(i, argc))
            {
                result.ok = false;
                result.error = "Saga: --first-playable-summary-out requires a path";
                return result;
            }
            result.config.firstPlayableSummaryPath = argv[++i];
        }
        else if (arg == "--first-playable-keyboard-report")
        {
            if (!HasValue(i, argc))
            {
                result.ok = false;
                result.error = "Saga: --first-playable-keyboard-report requires a path";
                return result;
            }
            result.config.firstPlayableKeyboardReportPath = argv[++i];
        }
        else if (arg == "--runtime-executable")
        {
            if (!HasValue(i, argc))
            {
                result.ok = false;
                result.error = "Saga: --runtime-executable requires a path";
                return result;
            }
            result.config.runtimeExecutable = argv[++i];
        }
        else if (arg == "--runtime-bridge-assembly")
        {
            if (!HasValue(i, argc))
            {
                result.ok = false;
                result.error = "Saga: --runtime-bridge-assembly requires a path";
                return result;
            }
            result.config.runtimeBridgeAssembly = argv[++i];
        }
        else if (arg == "--first-playable-timeout-ms")
        {
            if (!HasValue(i, argc))
            {
                result.ok = false;
                result.error = "Saga: --first-playable-timeout-ms requires a positive integer";
                return result;
            }
            try
            {
                result.config.firstPlayableTimeoutMs = std::stoi(argv[++i]);
            }
            catch (...)
            {
                result.ok = false;
                result.error = "Saga: --first-playable-timeout-ms requires a positive integer";
                return result;
            }
            if (result.config.firstPlayableTimeoutMs <= 0)
            {
                result.ok = false;
                result.error = "Saga: --first-playable-timeout-ms requires a positive integer";
                return result;
            }
        }
        else if (arg == "--first-playable-human-timeout-ms" ||
                 arg == "--first-playable-human-frames")
        {
            if (!HasValue(i, argc))
            {
                result.ok = false;
                result.error = "Saga: " + arg + " requires a positive integer";
                return result;
            }
            int value = 0;
            try
            {
                value = std::stoi(argv[++i]);
            }
            catch (...)
            {
                result.ok = false;
                result.error = "Saga: " + arg + " requires a positive integer";
                return result;
            }
            if (value <= 0)
            {
                result.ok = false;
                result.error = "Saga: " + arg + " requires a positive integer";
                return result;
            }
            if (arg == "--first-playable-human-timeout-ms")
                result.config.firstPlayableHumanTimeoutMs = value;
            else result.config.firstPlayableHumanFrames = value;
        }
        else if (arg == "--local-workspace-transaction-smoke")
        {
            result.config.localWorkspaceTransactionSmoke = true;
        }
        else if (arg == "--local-workspace-presence-lock-smoke")
        {
            result.config.localWorkspacePresenceLockSmoke = true;
        }
        else if (arg == "--local-workspace-review-smoke")
        {
            result.config.localWorkspaceReviewSmoke = true;
        }
        else if (arg == "--local-workspace-role-smoke")
        {
            result.config.localWorkspaceRoleSmoke = true;
        }
        else if (arg == "--local-workspace-slice-smoke")
        {
            result.config.localWorkspaceSliceSmoke = true;
        }
        else if (arg == "--local-workspace-approval-gate-smoke")
        {
            result.config.localWorkspaceApprovalGateSmoke = true;
        }
        else if (arg == "--actor")
        {
            if (!HasValue(i, argc))
            {
                result.ok = false;
                result.error = "Saga: --actor requires an id";
                return result;
            }
            result.config.localWorkspaceActorId = argv[++i];
        }
        else if (arg == "--operation")
        {
            if (!HasValue(i, argc))
            {
                result.ok = false;
                result.error = "Saga: --operation requires a kind";
                return result;
            }
            result.config.localWorkspaceOperationKind = argv[++i];
        }
        else if (arg == "--transaction-report-out")
        {
            if (!HasValue(i, argc))
            {
                result.ok = false;
                result.error =
                    "Saga: --transaction-report-out requires a path";
                return result;
            }
            result.config.localWorkspaceTransactionReportPath =
                std::filesystem::path(argv[++i]);
        }
        else if (arg == "--lock-target")
        {
            if (!HasValue(i, argc))
            {
                result.ok = false;
                result.error = "Saga: --lock-target requires a path";
                return result;
            }
            result.config.localWorkspaceLockTargetPath =
                std::filesystem::path(argv[++i]);
        }
        else if (arg == "--presence-lock-report-out")
        {
            if (!HasValue(i, argc))
            {
                result.ok = false;
                result.error =
                    "Saga: --presence-lock-report-out requires a path";
                return result;
            }
            result.config.localWorkspacePresenceLockReportPath =
                std::filesystem::path(argv[++i]);
        }
        else if (arg == "--review-target")
        {
            if (!HasValue(i, argc))
            {
                result.ok = false;
                result.error = "Saga: --review-target requires a path";
                return result;
            }
            result.config.localWorkspaceReviewTargetPath =
                std::filesystem::path(argv[++i]);
        }
        else if (arg == "--comment")
        {
            if (!HasValue(i, argc))
            {
                result.ok = false;
                result.error = "Saga: --comment requires text";
                return result;
            }
            result.config.localWorkspaceReviewComment = argv[++i];
        }
        else if (arg == "--review-report-out")
        {
            if (!HasValue(i, argc))
            {
                result.ok = false;
                result.error =
                    "Saga: --review-report-out requires a path";
                return result;
            }
            result.config.localWorkspaceReviewReportPath =
                std::filesystem::path(argv[++i]);
        }
        else if (arg == "--role")
        {
            if (!HasValue(i, argc))
            {
                result.ok = false;
                result.error = "Saga: --role requires a name";
                return result;
            }
            result.config.localWorkspaceRoleName = argv[++i];
        }
        else if (arg == "--permission")
        {
            if (!HasValue(i, argc))
            {
                result.ok = false;
                result.error = "Saga: --permission requires a name";
                return result;
            }
            result.config.localWorkspacePermissionName = argv[++i];
        }
        else if (arg == "--role-report-out")
        {
            if (!HasValue(i, argc))
            {
                result.ok = false;
                result.error =
                    "Saga: --role-report-out requires a path";
                return result;
            }
            result.config.localWorkspaceRoleReportPath =
                std::filesystem::path(argv[++i]);
        }
        else if (arg == "--slice")
        {
            if (!HasValue(i, argc))
            {
                result.ok = false;
                result.error = "Saga: --slice requires a name";
                return result;
            }
            result.config.localWorkspaceSliceName = argv[++i];
        }
        else if (arg == "--slice-target")
        {
            if (!HasValue(i, argc))
            {
                result.ok = false;
                result.error = "Saga: --slice-target requires a path";
                return result;
            }
            result.config.localWorkspaceSliceTargetPath =
                std::filesystem::path(argv[++i]);
        }
        else if (arg == "--slice-report-out")
        {
            if (!HasValue(i, argc))
            {
                result.ok = false;
                result.error =
                    "Saga: --slice-report-out requires a path";
                return result;
            }
            result.config.localWorkspaceSliceReportPath =
                std::filesystem::path(argv[++i]);
        }
        else if (arg == "--gate-target")
        {
            if (!HasValue(i, argc))
            {
                result.ok = false;
                result.error = "Saga: --gate-target requires a path";
                return result;
            }
            result.config.localWorkspaceGateTargetPath =
                std::filesystem::path(argv[++i]);
        }
        else if (arg == "--approval-state")
        {
            if (!HasValue(i, argc))
            {
                result.ok = false;
                result.error = "Saga: --approval-state requires a state";
                return result;
            }
            result.config.localWorkspaceApprovalState = argv[++i];
        }
        else if (arg == "--approval-gate-report-out")
        {
            if (!HasValue(i, argc))
            {
                result.ok = false;
                result.error =
                    "Saga: --approval-gate-report-out requires a path";
                return result;
            }
            result.config.localWorkspaceApprovalGateReportPath =
                std::filesystem::path(argv[++i]);
        }
        else if (arg == "--forge")
        {
            if (!HasValue(i, argc))
            {
                result.ok = false;
                result.error = "Saga: --forge requires a path";
                return result;
            }
            result.config.forgeExecutable = std::filesystem::path(argv[++i]);
        }
        else if (arg == "--sagascript-tool")
        {
            if (!HasValue(i, argc))
            {
                result.ok = false;
                result.error = "Saga: --sagascript-tool requires a path";
                return result;
            }
            result.config.sagaScriptExecutable =
                std::filesystem::path(argv[++i]);
        }
        else if (arg == "--prepare-only")
        {
            result.config.prepareOnly = true;
        }
        else if (IsQtOptionWithValue(arg))
        {
            if (!HasValue(i, argc))
            {
                result.ok = false;
                result.error = "Saga: " + arg + " requires a value";
                return result;
            }
            ++i;
        }
        else if (IsQtOptionWithInlineValue(arg))
        {
            continue;
        }
        else
        {
            result.ok = false;
            result.error = "Saga: unknown argument '" + arg + "'";
            return result;
        }
    }

    if (result.config.firstPlayableHumanCapture &&
        (result.config.firstPlayableCheck || result.config.workflowSmoke ||
         result.config.validateSagaScript || result.config.stagePackages ||
         result.config.publishCheck || result.config.prepareOnly ||
         result.config.localWorkspaceTransactionSmoke ||
         result.config.localWorkspacePresenceLockSmoke ||
         result.config.localWorkspaceReviewSmoke ||
         result.config.localWorkspaceRoleSmoke ||
         result.config.localWorkspaceSliceSmoke ||
         result.config.localWorkspaceApprovalGateSmoke ||
         result.config.target != SagaProductTargetKind::Editor))
    {
        result.ok = false;
        result.error = "Saga: --first-playable-human-capture conflicts with another command mode";
    }
    return result;
}

SagaProductInfo MakeBuiltProductInfo()
{
    SagaProductInfo info;
    info.distributionVersion = SAGA_PRODUCT_VERSION;
    info.buildVersion = SAGA_PRODUCT_VERSION;
    info.gitCommit = SAGA_PRODUCT_GIT_COMMIT;
    info.platform = SAGA_PRODUCT_PLATFORM;
    info.binaries = { "Saga" };
    return info;
}

SagaProductInfo LoadProductInfo(const SagaAppConfig& config,
                                std::string& outError)
{
    SagaProductInfo fallback = MakeBuiltProductInfo();
    outError.clear();

    if (config.versionInfoPath.empty() ||
        !std::filesystem::exists(config.versionInfoPath))
    {
        return fallback;
    }

    std::ifstream input(config.versionInfoPath);
    if (!input.is_open())
    {
        outError = "Cannot open product version info: " +
            config.versionInfoPath.string();
        return fallback;
    }

    nlohmann::json json;
    try
    {
        input >> json;
    }
    catch (const nlohmann::json::exception& e)
    {
        outError = std::string("Cannot parse product version info: ") + e.what();
        return fallback;
    }

    SagaProductInfo info;
    info.productName = ReadString(json, "productName", fallback.productName);
    info.distributionVersion =
        ReadString(json, "distributionVersion", fallback.distributionVersion);
    info.buildVersion = ReadString(json, "buildVersion", fallback.buildVersion);
    info.gitCommit = ReadString(json, "gitCommit", fallback.gitCommit);
    info.platform = ReadString(json, "platform", fallback.platform);
    info.binaries = ReadStringArray(json, "binaries");
    if (info.binaries.empty())
    {
        info.binaries = fallback.binaries;
    }
    return info;
}

} // namespace SagaProduct
