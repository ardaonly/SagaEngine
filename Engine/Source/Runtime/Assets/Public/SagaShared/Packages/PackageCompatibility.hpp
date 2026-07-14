/// @file PackageCompatibility.hpp
/// @brief Shared package compatibility metadata.

#pragma once

#include <map>
#include <string>
#include <vector>

namespace SagaShared::Packages
{

/// Runtime, engine, profile, and platform compatibility hints for a package.
struct PackageCompatibility
{
    std::string runtimeVersion;
    std::string engineVersion;
    std::string targetPlatform;
    std::vector<std::string> supportedBuildProfiles;
    std::map<std::string, std::string> metadata;
};

} // namespace SagaShared::Packages
