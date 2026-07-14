/// @file AssetManifestLoader.hpp
/// @brief Loads and validates runtime-ready asset manifest files.

#pragma once

#include <SagaEngine/Assets/AssetManifest.hpp>

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace SagaEngine::Assets
{

/// Structured validation error emitted while reading an asset manifest.
struct AssetManifestError
{
    std::string diagnosticId;                         ///< Stable Runtime.Asset.* diagnostic id.
    std::string message;                              ///< Human-readable validation message.
    std::filesystem::path manifestPath;               ///< Manifest file that produced the error.
    std::optional<std::size_t> assetIndex;            ///< Asset array index when applicable.
    std::optional<std::string> fieldName;             ///< Field involved in the validation failure.
    std::optional<std::filesystem::path> resolvedPath; ///< Resolved asset path when applicable.
};

/// Result of a runtime asset manifest load attempt.
struct AssetManifestLoadResult
{
    AssetManifest manifest;                    ///< Parsed data; only complete when Succeeded is true.
    std::vector<AssetManifestError> errors;    ///< Structured validation and I/O failures.

    /// Return true when the manifest loaded without validation errors.
    [[nodiscard]] bool Succeeded() const noexcept { return errors.empty(); }
};

/// Optional validation controls for runtime asset manifest loading.
struct AssetManifestLoadOptions
{
    bool validateAssetFiles = false;                ///< Check that each referenced cooked asset file exists.
    std::filesystem::path assetBaseDirectory;       ///< Base path for relative asset references.
};

/// Reads runtime asset manifests without owning import, cook, or package staging.
class AssetManifestLoader
{
public:
    /// Load and validate an asset manifest from disk.
    [[nodiscard]] static AssetManifestLoadResult LoadFromFile(
        const std::filesystem::path& manifestPath);

    /// Load and validate an asset manifest using explicit validation options.
    [[nodiscard]] static AssetManifestLoadResult LoadFromFile(
        const std::filesystem::path& manifestPath,
        const AssetManifestLoadOptions& options);
};

namespace AssetManifestDiagnostics
{

inline constexpr const char* ManifestMissing =
    "Runtime.Asset.ManifestMissing";
inline constexpr const char* ParseFailed =
    "Runtime.Asset.ParseFailed";
inline constexpr const char* UnsupportedVersion =
    "Runtime.Asset.UnsupportedVersion";
inline constexpr const char* MissingField =
    "Runtime.Asset.MissingField";
inline constexpr const char* InvalidField =
    "Runtime.Asset.InvalidField";
inline constexpr const char* UnknownKind =
    "Runtime.Asset.UnknownKind";
inline constexpr const char* DuplicateId =
    "Runtime.Asset.DuplicateId";
inline constexpr const char* InvalidPath =
    "Runtime.Asset.InvalidPath";
inline constexpr const char* FileMissing =
    "Runtime.Asset.FileMissing";

} // namespace AssetManifestDiagnostics

} // namespace SagaEngine::Assets
