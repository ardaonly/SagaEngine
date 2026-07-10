/// @file ArtifactManifestLoader.hpp
/// @brief Loads and validates runtime artifact manifest files.

#pragma once

#include <SagaEngine/Artifacts/ArtifactManifest.hpp>

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace SagaEngine::Artifacts
{

/// Structured validation error emitted while reading an artifact manifest.
struct ArtifactManifestError
{
    std::string diagnosticId;                         ///< Stable Runtime.Artifact.* diagnostic id.
    std::string message;                              ///< Human-readable validation message.
    std::filesystem::path manifestPath;               ///< Manifest file that produced the error.
    std::optional<std::size_t> artifactIndex;         ///< Artifact array index when applicable.
    std::optional<std::string> fieldName;             ///< Field involved in the validation failure.
};

/// Result of a runtime artifact manifest load attempt.
struct ArtifactManifestLoadResult
{
    ArtifactManifest manifest;                   ///< Parsed data; only complete when Succeeded is true.
    std::vector<ArtifactManifestError> errors;   ///< Structured validation and I/O failures.

    /// Return true when the manifest loaded without validation errors.
    [[nodiscard]] bool Succeeded() const noexcept { return errors.empty(); }
};

/// Optional validation controls for runtime artifact manifest loading.
struct ArtifactManifestLoadOptions
{
    bool validateArtifactFiles = false;              ///< Check that each referenced artifact file exists.
    std::filesystem::path artifactBaseDirectory;     ///< Base path for relative artifact references.
};

/// Reads runtime artifact manifests without depending on build-tool internals.
class ArtifactManifestLoader
{
public:
    /// Load and validate an artifact manifest from disk.
    [[nodiscard]] static ArtifactManifestLoadResult LoadFromFile(
        const std::filesystem::path& manifestPath);

    /// Load and validate an artifact manifest using explicit validation options.
    [[nodiscard]] static ArtifactManifestLoadResult LoadFromFile(
        const std::filesystem::path& manifestPath,
        const ArtifactManifestLoadOptions& options);
};

namespace ArtifactManifestDiagnostics
{

inline constexpr const char* ManifestMissing =
    "Runtime.Artifact.ManifestMissing";
inline constexpr const char* ParseFailed =
    "Runtime.Artifact.ParseFailed";
inline constexpr const char* UnsupportedVersion =
    "Runtime.Artifact.UnsupportedVersion";
inline constexpr const char* MissingField =
    "Runtime.Artifact.MissingField";
inline constexpr const char* InvalidField =
    "Runtime.Artifact.InvalidField";
inline constexpr const char* UnknownKind =
    "Runtime.Artifact.UnknownKind";
inline constexpr const char* FileMissing =
    "Runtime.Artifact.FileMissing";
inline constexpr const char* DuplicateId =
    "Runtime.Artifact.DuplicateId";

} // namespace ArtifactManifestDiagnostics

} // namespace SagaEngine::Artifacts
