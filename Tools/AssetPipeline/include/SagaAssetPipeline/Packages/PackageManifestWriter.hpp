/// @file PackageManifestWriter.hpp
/// @brief Writes AssetPipeline package metadata to the Runtime package schema.

#pragma once

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace SagaAssetPipeline {

/// Tool-side package destination token.
enum class PackageManifestKind
{
    Invalid = 0,
    Client,
    Server,
};

/// Tool-side reference to a generated package manifest artifact.
struct PackageManifestRefWriteInput
{
    std::string id;                   ///< Stable manifest reference id.
    std::string path;                 ///< Package-relative manifest path.
    std::optional<std::string> hash;  ///< Optional manifest integrity hash.
};

/// Tool-side package manifest write input.
struct PackageManifestWriteInput
{
    std::string packageId;                             ///< Stable package id.
    PackageManifestKind packageKind = PackageManifestKind::Invalid; ///< Client or server destination.
    std::string buildProfile;                          ///< Build profile token.
    std::string targetPlatform;                        ///< Target platform token.
    std::string runtimeCompatibilityVersion;           ///< Runtime compatibility token.
    std::optional<std::string> assetIdentityManifest;  ///< Optional package-relative identity manifest path.
    std::vector<PackageManifestRefWriteInput> assetManifests; ///< Runtime asset manifest references.
    std::vector<PackageManifestRefWriteInput> artifactManifests; ///< Runtime artifact manifest references.
    std::optional<std::string> packageHash;            ///< Optional package integrity hash.
};

/// Structured diagnostic emitted while writing a package manifest.
struct PackageManifestWriteDiagnostic
{
    std::string diagnosticId;                         ///< Stable AssetPipeline.PackageManifestWriter.* diagnostic id.
    std::string message;                              ///< Human-readable diagnostic detail.
    std::filesystem::path outputPath;                 ///< Output manifest path involved in the diagnostic.
    std::optional<std::size_t> referenceIndex;        ///< Manifest reference index when applicable.
    std::optional<std::string> fieldName;             ///< Field involved in validation when applicable.
    std::string referenceId;                          ///< Manifest reference id involved in the diagnostic.
    std::filesystem::path referencePath;              ///< Manifest reference path involved in the diagnostic.
};

/// Result of writing a package manifest.
struct PackageManifestWriteResult
{
    std::size_t writtenAssetManifestCount = 0;        ///< Number of asset manifest refs written when successful.
    std::size_t writtenArtifactManifestCount = 0;     ///< Number of artifact manifest refs written when successful.
    std::vector<PackageManifestWriteDiagnostic> diagnostics; ///< Write/validation diagnostics.

    /// Return true when the manifest was written without diagnostics.
    [[nodiscard]] bool Succeeded() const noexcept { return diagnostics.empty(); }
};

/// Writes Runtime-compatible package manifests from AssetPipeline metadata.
class PackageManifestWriter
{
public:
    /// Write package metadata to disk as the Runtime package manifest schema.
    [[nodiscard]] static PackageManifestWriteResult WriteToFile(
        const std::filesystem::path& outputPath,
        const PackageManifestWriteInput& input);
};

namespace PackageManifestWriterDiagnostics {

inline constexpr const char* InvalidPackageId =
    "AssetPipeline.PackageManifestWriter.InvalidPackageId";
inline constexpr const char* InvalidPackageKind =
    "AssetPipeline.PackageManifestWriter.InvalidPackageKind";
inline constexpr const char* InvalidBuildProfile =
    "AssetPipeline.PackageManifestWriter.InvalidBuildProfile";
inline constexpr const char* InvalidTargetPlatform =
    "AssetPipeline.PackageManifestWriter.InvalidTargetPlatform";
inline constexpr const char* InvalidRuntimeCompatibilityVersion =
    "AssetPipeline.PackageManifestWriter.InvalidRuntimeCompatibilityVersion";
inline constexpr const char* InvalidAssetIdentityManifestPath =
    "AssetPipeline.PackageManifestWriter.InvalidAssetIdentityManifestPath";
inline constexpr const char* InvalidManifestRefId =
    "AssetPipeline.PackageManifestWriter.InvalidManifestRefId";
inline constexpr const char* DuplicateManifestRefId =
    "AssetPipeline.PackageManifestWriter.DuplicateManifestRefId";
inline constexpr const char* InvalidManifestRefPath =
    "AssetPipeline.PackageManifestWriter.InvalidManifestRefPath";
inline constexpr const char* OutputDirectoryFailed =
    "AssetPipeline.PackageManifestWriter.OutputDirectoryFailed";
inline constexpr const char* OutputOpenFailed =
    "AssetPipeline.PackageManifestWriter.OutputOpenFailed";
inline constexpr const char* OutputWriteFailed =
    "AssetPipeline.PackageManifestWriter.OutputWriteFailed";

} // namespace PackageManifestWriterDiagnostics

} // namespace SagaAssetPipeline
