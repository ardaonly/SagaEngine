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
        "  --workspace <path|builtin:basic>  SDE workspace definition root\n"
        "  --target <editor|runtime|server>  Product role to prepare\n"
        "  --version-info <path>             Distribution version.json path\n"
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
