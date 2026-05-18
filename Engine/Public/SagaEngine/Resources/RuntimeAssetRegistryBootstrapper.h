/// @file RuntimeAssetRegistryBootstrapper.h
/// @brief Registers package asset manifests into the runtime asset registry.

#pragma once

#include "SagaEngine/Packages/PackageManifest.hpp"
#include "SagaEngine/Resources/AssetIdResolver.h"
#include "SagaEngine/Resources/AssetManifestRegistryAdapter.h"
#include "SagaEngine/Resources/AssetRegistry.h"

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace SagaEngine::Resources {

/// Options for package asset registry bootstrap.
struct RuntimeAssetRegistryBootstrapOptions
{
    std::filesystem::path packageManifestPath; ///< Startup package manifest path used for resolving references.
    bool validateAssetFiles = true;            ///< Validate cooked asset files while loading asset manifests.
};

/// Structured diagnostic emitted while bootstrapping package assets into a registry.
struct RuntimeAssetRegistryBootstrapDiagnostic
{
    std::string diagnosticId;                         ///< Stable diagnostic id from bootstrap, loader, or adapter.
    std::string message;                              ///< Human-readable diagnostic message.
    std::filesystem::path manifestPath;               ///< Manifest that owns the diagnostic.
    std::optional<std::size_t> referenceIndex;        ///< Package asset manifest reference index when available.
    std::optional<std::size_t> assetIndex;            ///< Asset manifest entry index when available.
    AssetKey assetKey;                                ///< Asset key involved in the diagnostic.
    AssetId assetId = kInvalidAssetId;                ///< Asset id involved in the diagnostic.
    std::optional<std::filesystem::path> resolvedPath; ///< Resolved path involved in the diagnostic.
};

/// Result of validating or registering one package's asset manifests.
struct RuntimeAssetRegistryBootstrapResult
{
    std::size_t registeredAssetCount = 0;                       ///< Number of registry entries inserted, if registration ran.
    std::vector<RuntimeAssetRegistryBootstrapDiagnostic> diagnostics; ///< Bootstrap diagnostics.

    /// Return true when package asset processing completed without diagnostics.
    [[nodiscard]] bool Succeeded() const noexcept { return diagnostics.empty(); }
};

/// Validates package asset manifests and populates AssetRegistry when requested.
class RuntimeAssetRegistryBootstrapper
{
public:
    /// Validate package asset identity coverage without mutating a registry.
    [[nodiscard]] static RuntimeAssetRegistryBootstrapResult
    ValidatePackageAssetsFromPackageIdentityManifest(
        const Packages::PackageManifest& packageManifest,
        const RuntimeAssetRegistryBootstrapOptions& options);

    /// Register package assets using the package's assetIdentityManifest reference.
    [[nodiscard]] static RuntimeAssetRegistryBootstrapResult
    RegisterPackageAssetsFromPackageIdentityManifest(
        AssetRegistry& registry,
        const Packages::PackageManifest& packageManifest,
        const RuntimeAssetRegistryBootstrapOptions& options);

    /// Register package assets after preflighting the complete package asset plan.
    [[nodiscard]] static RuntimeAssetRegistryBootstrapResult RegisterPackageAssets(
        AssetRegistry& registry,
        const Packages::PackageManifest& packageManifest,
        const IAssetIdResolver& assetIdResolver,
        const RuntimeAssetRegistryBootstrapOptions& options);
};

namespace RuntimeAssetRegistryBootstrapDiagnostics {

inline constexpr const char* DuplicatePackageAssetKey =
    "Runtime.AssetRegistryBootstrap.DuplicatePackageAssetKey";
inline constexpr const char* DuplicatePackageAssetId =
    "Runtime.AssetRegistryBootstrap.DuplicatePackageAssetId";
inline constexpr const char* RegistryAssetKeyCollision =
    "Runtime.AssetRegistryBootstrap.RegistryAssetKeyCollision";
inline constexpr const char* RegistryAssetIdCollision =
    "Runtime.AssetRegistryBootstrap.RegistryAssetIdCollision";
inline constexpr const char* MissingAssetIdentityManifest =
    "Runtime.AssetRegistryBootstrap.MissingAssetIdentityManifest";

} // namespace RuntimeAssetRegistryBootstrapDiagnostics

} // namespace SagaEngine::Resources
