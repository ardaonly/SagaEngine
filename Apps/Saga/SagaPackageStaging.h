/// @file SagaPackageStaging.h
/// @brief Product-owned package manifest staging for Saga projects.

#pragma once

#include "SagaSessionModel.h"

#include <filesystem>
#include <string>
#include <vector>

namespace SagaProduct
{

/// Product request for staging package manifests.
struct SagaPackageStagingRequest
{
    std::filesystem::path projectRoot;
    std::string profile = "shipping-full";
    std::string targetPlatform = "linux-x64";
    std::string runtimeCompatibilityVersion = "0.0.8";
    std::filesystem::path reportPath;
};

/// Paths written by the package staging service.
struct SagaPackageStagingPaths
{
    std::filesystem::path manifestDirectory;
    std::filesystem::path clientPackageManifest;
    std::filesystem::path serverPackageManifest;
    std::filesystem::path reportPath;
};

/// Product package staging outcome.
struct SagaPackageStagingResult
{
    bool ok = false;
    SagaPackageStagingPaths paths;
    std::vector<SagaProductDiagnostic> diagnostics;
};

/// Product-owned service that materializes runtime/server package manifests.
class SagaPackageStagingService
{
public:
    [[nodiscard]] SagaPackageStagingResult Stage(
        const SagaPackageStagingRequest& request) const;

    [[nodiscard]] static SagaPackageStagingPaths DefaultPathsForProject(
        const std::filesystem::path& projectRoot);
};

} // namespace SagaProduct
