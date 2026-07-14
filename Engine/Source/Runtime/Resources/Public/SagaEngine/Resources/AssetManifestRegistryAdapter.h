/// @file AssetManifestRegistryAdapter.h
/// @brief Registers validated asset manifests into the runtime asset registry.

#pragma once

#include "SagaEngine/Assets/AssetManifest.hpp"
#include "SagaEngine/Resources/AssetIdResolver.h"
#include "SagaEngine/Resources/AssetRegistry.h"

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace SagaEngine::Resources {

/// Options that control manifest-to-registry registration.
struct AssetManifestRegistryAdapterOptions
{
    std::filesystem::path assetManifestPath;  ///< Manifest path used for diagnostics and relative path resolution.
    std::filesystem::path assetBaseDirectory; ///< Optional base path for runtime/cooked asset paths.
};

/// Structured diagnostic emitted while adapting manifest entries into registry entries.
struct AssetManifestRegistryAdapterDiagnostic
{
    std::string diagnosticId;                         ///< Stable Runtime.AssetRegistryAdapter.* diagnostic id.
    std::string message;                              ///< Human-readable diagnostic message.
    AssetKey assetKey;                                ///< Manifest asset key involved in the diagnostic.
    AssetId assetId = kInvalidAssetId;                ///< Resolved runtime id when available.
    std::optional<std::size_t> assetIndex;            ///< Manifest asset index when available.
    std::filesystem::path manifestPath;               ///< Manifest path supplied to the adapter.
    std::optional<std::filesystem::path> resolvedPath; ///< Resolved runtime/cooked asset path when available.
};

/// Result of adapting one manifest into the registry.
struct AssetManifestRegistryAdapterResult
{
    std::size_t registeredAssetCount = 0;                    ///< Number of entries inserted into the registry.
    std::vector<AssetManifestRegistryAdapterDiagnostic> diagnostics; ///< Adapter diagnostics.

    /// Return true when every manifest entry was registered.
    [[nodiscard]] bool Succeeded() const noexcept { return diagnostics.empty(); }
};

/// One registry entry planned from a manifest asset entry.
struct AssetManifestRegistryAdapterPlannedEntry
{
    AssetRegistryEntry entry;                         ///< Registry entry ready for batch insertion.
    std::optional<std::size_t> assetIndex;            ///< Source manifest asset index.
    std::optional<std::filesystem::path> resolvedPath; ///< Resolved runtime/cooked asset path.
};

/// Result of adapting one manifest without mutating a registry.
struct AssetManifestRegistryAdapterPlanResult
{
    std::vector<AssetManifestRegistryAdapterPlannedEntry> plannedEntries; ///< Valid entries to insert later.
    std::vector<AssetManifestRegistryAdapterDiagnostic> diagnostics;      ///< Adapter diagnostics.

    /// Return true when every manifest entry can be represented as a registry entry.
    [[nodiscard]] bool Succeeded() const noexcept { return diagnostics.empty(); }
};

/// Converts validated runtime asset manifest data into AssetRegistry entries.
class AssetManifestRegistryAdapter
{
public:
    /// Build registry entries without mutating a registry.
    [[nodiscard]] static AssetManifestRegistryAdapterPlanResult PlanManifestAssets(
        const Assets::AssetManifest& manifest,
        const IAssetIdResolver& assetIdResolver,
        const AssetManifestRegistryAdapterOptions& options);

    /// Register all manifest assets after preflighting identity and registry collisions.
    [[nodiscard]] static AssetManifestRegistryAdapterResult RegisterManifestAssets(
        AssetRegistry& registry,
        const Assets::AssetManifest& manifest,
        const IAssetIdResolver& assetIdResolver,
        const AssetManifestRegistryAdapterOptions& options);
};

namespace AssetManifestRegistryAdapterDiagnostics {

inline constexpr const char* InvalidAssetKey =
    "Runtime.AssetRegistryAdapter.InvalidAssetKey";
inline constexpr const char* DuplicateManifestAssetKey =
    "Runtime.AssetRegistryAdapter.DuplicateManifestAssetKey";
inline constexpr const char* MissingAssetIdMapping =
    "Runtime.AssetRegistryAdapter.MissingAssetIdMapping";
inline constexpr const char* InvalidResolvedAssetId =
    "Runtime.AssetRegistryAdapter.InvalidResolvedAssetId";
inline constexpr const char* DuplicateResolvedAssetId =
    "Runtime.AssetRegistryAdapter.DuplicateResolvedAssetId";
inline constexpr const char* RegistryAssetKeyCollision =
    "Runtime.AssetRegistryAdapter.RegistryAssetKeyCollision";
inline constexpr const char* RegistryAssetIdCollision =
    "Runtime.AssetRegistryAdapter.RegistryAssetIdCollision";
inline constexpr const char* UnsupportedKind =
    "Runtime.AssetRegistryAdapter.UnsupportedKind";

} // namespace AssetManifestRegistryAdapterDiagnostics

} // namespace SagaEngine::Resources
