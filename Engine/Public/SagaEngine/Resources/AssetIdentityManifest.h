/// @file AssetIdentityManifest.h
/// @brief Defines package AssetKey to runtime AssetId mapping manifest contracts.

#pragma once

#include "SagaEngine/Resources/AssetIdResolver.h"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace SagaEngine::Resources {

/// One explicit package asset identity mapping.
struct AssetIdentityMapping
{
    AssetKey assetKey;                  ///< Canonical package-facing asset key.
    AssetId  assetId = kInvalidAssetId; ///< Runtime numeric id; zero is reserved invalid.
};

/// Parsed package asset identity manifest.
struct AssetIdentityManifest
{
    std::uint32_t schemaVersion = 0;               ///< Manifest schema version accepted by the loader.
    std::vector<AssetIdentityMapping> mappings;    ///< Explicit package asset identity mappings.
};

/// Structured validation error emitted while reading an asset identity manifest.
struct AssetIdentityManifestError
{
    std::string diagnosticId;                  ///< Stable Runtime.AssetIdentityManifest.* diagnostic id.
    std::string message;                       ///< Human-readable validation message.
    std::filesystem::path manifestPath;        ///< Manifest file that produced the error.
    std::optional<std::size_t> mappingIndex;   ///< Mapping index when applicable.
    std::optional<std::string> fieldName;      ///< Field involved in the validation failure.
    AssetKey assetKey;                         ///< Asset key involved in the diagnostic.
    AssetId assetId = kInvalidAssetId;         ///< Asset id involved in the diagnostic.
};

/// Result of a runtime asset identity manifest load attempt.
struct AssetIdentityManifestLoadResult
{
    AssetIdentityManifest manifest;                 ///< Parsed data; only complete when Succeeded is true.
    std::vector<AssetIdentityManifestError> errors; ///< Structured validation and I/O failures.

    /// Return true when the manifest loaded without validation errors.
    [[nodiscard]] bool Succeeded() const noexcept { return errors.empty(); }
};

/// Reads package asset identity manifests without owning asset import or cook policy.
class AssetIdentityManifestLoader
{
public:
    /// Load and validate an asset identity manifest from disk.
    [[nodiscard]] static AssetIdentityManifestLoadResult LoadFromFile(
        const std::filesystem::path& manifestPath);

    /// Build an explicit in-memory resolver from a validated identity manifest.
    [[nodiscard]] static StaticAssetIdResolver BuildResolver(
        const AssetIdentityManifest& manifest);
};

namespace AssetIdentityManifestDiagnostics {

inline constexpr const char* ManifestMissing =
    "Runtime.AssetIdentityManifest.ManifestMissing";
inline constexpr const char* ParseFailed =
    "Runtime.AssetIdentityManifest.ParseFailed";
inline constexpr const char* UnsupportedVersion =
    "Runtime.AssetIdentityManifest.UnsupportedVersion";
inline constexpr const char* MissingField =
    "Runtime.AssetIdentityManifest.MissingField";
inline constexpr const char* InvalidField =
    "Runtime.AssetIdentityManifest.InvalidField";
inline constexpr const char* DuplicateAssetKey =
    "Runtime.AssetIdentityManifest.DuplicateAssetKey";
inline constexpr const char* DuplicateAssetId =
    "Runtime.AssetIdentityManifest.DuplicateAssetId";

} // namespace AssetIdentityManifestDiagnostics

} // namespace SagaEngine::Resources
