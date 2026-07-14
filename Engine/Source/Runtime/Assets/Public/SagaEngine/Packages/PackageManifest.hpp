/// @file PackageManifest.hpp
/// @brief Defines runtime package manifest value types.

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace SagaEngine::Packages
{

/// Runtime package execution destination.
enum class PackageKind : std::uint8_t
{
    Client = 0,
    Server,
};

/// Convert a manifest kind token into a runtime package kind.
[[nodiscard]] std::optional<PackageKind> TryParsePackageKind(std::string_view value) noexcept;

/// Return the stable manifest token for a runtime package kind.
[[nodiscard]] std::string_view ToString(PackageKind kind) noexcept;

/// Package-relative reference to another manifest.
struct PackageManifestRef
{
    std::string id;                   ///< Stable manifest id inside the package.
    std::string path;                 ///< Package-relative manifest path.
    std::optional<std::string> hash;  ///< Optional integrity hash reserved for later validation.
};

/// Parsed package manifest consumed by runtime or server startup validation.
struct PackageManifest
{
    std::uint32_t schemaVersion = 0;                    ///< Manifest schema version accepted by the loader.
    std::string packageId;                              ///< Stable package id.
    PackageKind packageKind = PackageKind::Client;      ///< Client or server package destination.
    std::string buildProfile;                           ///< Build profile token such as dev-client.
    std::string targetPlatform;                         ///< Target platform token.
    std::string runtimeCompatibilityVersion;            ///< Runtime compatibility version token.
    std::optional<std::string> assetIdentityManifest;   ///< Optional package-relative AssetKey to AssetId mapping manifest.
    std::vector<PackageManifestRef> assetManifests;     ///< Asset manifest references staged in the package.
    std::vector<PackageManifestRef> artifactManifests;  ///< Artifact manifest references staged in the package.
    std::optional<std::string> packageHash;             ///< Optional package integrity hash reserved for later validation.
};

} // namespace SagaEngine::Packages
