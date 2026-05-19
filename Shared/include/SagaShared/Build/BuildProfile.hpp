/// @file BuildProfile.hpp
/// @brief Shared build profile contract.

#pragma once

#include "SagaShared/Build/BuildConfiguration.hpp"
#include "SagaShared/Build/BuildProfileId.hpp"
#include "SagaShared/Build/TargetPlatform.hpp"
#include "SagaShared/Packages/PackageKind.hpp"

#include <map>
#include <string>

namespace SagaShared::Build
{

/// Data-only build profile descriptor shared by product, tools, and reports.
struct BuildProfile
{
    BuildProfileId profileId;
    std::string displayName;
    TargetPlatform targetPlatform;
    Packages::PackageKind outputPackageKind = Packages::PackageKind::Client;
    BuildConfiguration configuration;
    std::string sdeOptionsRef;
    std::string graphValidationPolicy;
    std::string scriptCompilePolicy;
    std::string assetCookPolicy;
    std::string packagePolicy;
    std::map<std::string, std::string> metadata;
};

} // namespace SagaShared::Build
