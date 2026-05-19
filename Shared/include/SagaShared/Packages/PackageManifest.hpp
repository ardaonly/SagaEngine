/// @file PackageManifest.hpp
/// @brief Shared package manifest contract.

#pragma once

#include "SagaShared/Artifacts/ArtifactRef.hpp"
#include "SagaShared/Packages/PackageCompatibility.hpp"
#include "SagaShared/Packages/PackageDependency.hpp"
#include "SagaShared/Packages/PackageId.hpp"
#include "SagaShared/Packages/PackageKind.hpp"
#include "SagaShared/Packages/PackageVersion.hpp"

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace SagaShared::Packages
{

/// Data-only package manifest shared by build, publish, runtime, and reporting paths.
struct PackageManifest
{
    std::uint32_t schemaVersion = 0;
    PackageId packageId;
    PackageKind packageKind = PackageKind::Client;
    std::string packageName;
    PackageVersion version;
    std::string buildProfile;
    std::string targetPlatform;
    std::vector<PackageDependency> dependencies;
    std::vector<Artifacts::ArtifactRef> artifacts;
    std::vector<Artifacts::ArtifactRef> assetManifests;
    std::vector<Artifacts::ArtifactRef> scriptManifests;
    std::vector<Artifacts::ArtifactRef> graphManifests;
    PackageCompatibility compatibility;
    std::map<std::string, std::string> metadata;
};

} // namespace SagaShared::Packages
