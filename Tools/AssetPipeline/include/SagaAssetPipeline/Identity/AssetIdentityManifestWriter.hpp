/// @file AssetIdentityManifestWriter.hpp
/// @brief Writes AssetPipeline identity mappings to the Runtime identity manifest schema.

#pragma once

#include "SagaAssetPipeline/Identity/AssetIdentityGenerator.hpp"

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace SagaAssetPipeline {

/// Structured diagnostic emitted while writing an asset identity manifest.
struct AssetIdentityManifestWriteDiagnostic
{
    std::string diagnosticId;                   ///< Stable AssetPipeline.AssetIdentityManifestWriter.* diagnostic id.
    std::string message;                        ///< Human-readable diagnostic detail.
    std::filesystem::path outputPath;           ///< Output manifest path involved in the diagnostic.
    std::optional<std::size_t> mappingIndex;    ///< Mapping index when applicable.
    std::optional<std::string> fieldName;       ///< Field involved in validation when applicable.
    std::string assetKey;                       ///< Asset key involved in the diagnostic.
    AssetId assetId = kInvalidAssetId;          ///< Asset id involved in the diagnostic.
};

/// Result of writing an asset identity manifest.
struct AssetIdentityManifestWriteResult
{
    std::size_t writtenMappingCount = 0;                  ///< Number of mappings written when successful.
    std::vector<AssetIdentityManifestWriteDiagnostic> diagnostics; ///< Write/validation diagnostics.

    /// Return true when the manifest was written without diagnostics.
    [[nodiscard]] bool Succeeded() const noexcept { return diagnostics.empty(); }
};

/// Writes generated asset identity mappings in the Runtime manifest schema.
class AssetIdentityManifestWriter
{
public:
    /// Write mappings to disk as `{ schemaVersion: 1, mappings: [...] }`.
    [[nodiscard]] static AssetIdentityManifestWriteResult WriteToFile(
        const std::filesystem::path& outputPath,
        const std::vector<AssetIdentityMapping>& mappings);
};

namespace AssetIdentityManifestWriterDiagnostics {

inline constexpr const char* InvalidAssetKey =
    "AssetPipeline.AssetIdentityManifestWriter.InvalidAssetKey";
inline constexpr const char* InvalidAssetId =
    "AssetPipeline.AssetIdentityManifestWriter.InvalidAssetId";
inline constexpr const char* DuplicateAssetKey =
    "AssetPipeline.AssetIdentityManifestWriter.DuplicateAssetKey";
inline constexpr const char* DuplicateAssetId =
    "AssetPipeline.AssetIdentityManifestWriter.DuplicateAssetId";
inline constexpr const char* OutputDirectoryFailed =
    "AssetPipeline.AssetIdentityManifestWriter.OutputDirectoryFailed";
inline constexpr const char* OutputOpenFailed =
    "AssetPipeline.AssetIdentityManifestWriter.OutputOpenFailed";
inline constexpr const char* OutputWriteFailed =
    "AssetPipeline.AssetIdentityManifestWriter.OutputWriteFailed";

} // namespace AssetIdentityManifestWriterDiagnostics

} // namespace SagaAssetPipeline
