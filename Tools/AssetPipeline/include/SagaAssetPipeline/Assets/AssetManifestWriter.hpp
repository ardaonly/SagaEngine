/// @file AssetManifestWriter.hpp
/// @brief Writes AssetPipeline asset entries to the Runtime asset manifest schema.

#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace SagaAssetPipeline {

/// Tool-side description of one runtime-ready asset manifest entry.
struct AssetManifestWriterEntry
{
    std::string id;                                  ///< Stable asset key/id emitted into the manifest.
    std::string kind;                                ///< Runtime asset kind token.
    std::string path;                                ///< Manifest-relative or package-relative cooked artifact path.
    std::optional<std::string> hash;                 ///< Optional integrity hash.
    std::vector<std::string> dependencies;           ///< Optional dependency asset keys.
    std::optional<std::uint64_t> memoryEstimateBytes; ///< Optional residency budget hint.
    std::optional<std::string> streamingGroup;       ///< Optional streaming policy group.
    std::optional<std::string> platform;             ///< Optional target platform token.
    std::optional<std::string> profile;              ///< Optional build/profile token.
};

/// Structured diagnostic emitted while writing an asset manifest.
struct AssetManifestWriteDiagnostic
{
    std::string diagnosticId;                 ///< Stable AssetPipeline.AssetManifestWriter.* diagnostic id.
    std::string message;                      ///< Human-readable diagnostic detail.
    std::filesystem::path outputPath;         ///< Output manifest path involved in the diagnostic.
    std::optional<std::size_t> assetIndex;    ///< Asset index when applicable.
    std::optional<std::string> fieldName;     ///< Field involved in validation when applicable.
    std::string assetId;                      ///< Asset id involved in the diagnostic.
    std::string assetKind;                    ///< Asset kind involved in the diagnostic.
    std::filesystem::path assetPath;          ///< Asset path involved in the diagnostic.
};

/// Result of writing an asset manifest.
struct AssetManifestWriteResult
{
    std::size_t writtenAssetCount = 0;                  ///< Number of assets written when successful.
    std::vector<AssetManifestWriteDiagnostic> diagnostics; ///< Write/validation diagnostics.

    /// Return true when the manifest was written without diagnostics.
    [[nodiscard]] bool Succeeded() const noexcept { return diagnostics.empty(); }
};

/// Writes runtime-ready asset manifest entries in the Runtime manifest schema.
class AssetManifestWriter
{
public:
    /// Write assets to disk as `{ schemaVersion: 1, assets: [...] }`.
    [[nodiscard]] static AssetManifestWriteResult WriteToFile(
        const std::filesystem::path& outputPath,
        const std::vector<AssetManifestWriterEntry>& assets);
};

namespace AssetManifestWriterDiagnostics {

inline constexpr const char* InvalidAssetId =
    "AssetPipeline.AssetManifestWriter.InvalidAssetId";
inline constexpr const char* InvalidAssetKind =
    "AssetPipeline.AssetManifestWriter.InvalidAssetKind";
inline constexpr const char* InvalidAssetPath =
    "AssetPipeline.AssetManifestWriter.InvalidAssetPath";
inline constexpr const char* DuplicateAssetId =
    "AssetPipeline.AssetManifestWriter.DuplicateAssetId";
inline constexpr const char* InvalidDependency =
    "AssetPipeline.AssetManifestWriter.InvalidDependency";
inline constexpr const char* OutputDirectoryFailed =
    "AssetPipeline.AssetManifestWriter.OutputDirectoryFailed";
inline constexpr const char* OutputOpenFailed =
    "AssetPipeline.AssetManifestWriter.OutputOpenFailed";
inline constexpr const char* OutputWriteFailed =
    "AssetPipeline.AssetManifestWriter.OutputWriteFailed";

} // namespace AssetManifestWriterDiagnostics

} // namespace SagaAssetPipeline
