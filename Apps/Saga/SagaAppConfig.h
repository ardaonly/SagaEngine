/// @file SagaAppConfig.h
/// @brief Product startup configuration and distribution metadata loading.

#pragma once

#include "SagaSessionModel.h"

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
    std::filesystem::path builtInWorkspaceRoot;
    std::string           workspaceSelector = "builtin:basic";
    SagaProductTargetKind target = SagaProductTargetKind::Editor;
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
