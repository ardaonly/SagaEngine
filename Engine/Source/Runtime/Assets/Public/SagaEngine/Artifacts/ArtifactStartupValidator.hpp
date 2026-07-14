/// @file ArtifactStartupValidator.hpp
/// @brief Validates runtime artifact manifests against startup acceptance policy.

#pragma once

#include <SagaEngine/Artifacts/ArtifactManifestLoader.hpp>

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace SagaEngine::Artifacts
{

/// Structured startup acceptance error for a runtime artifact manifest.
struct ArtifactStartupValidationError
{
    std::string diagnosticId;                         ///< Stable Runtime.Artifact.* diagnostic id.
    std::string message;                              ///< Human-readable startup validation message.
    std::filesystem::path manifestPath;               ///< Manifest file being validated.
    std::optional<std::size_t> artifactIndex;         ///< Artifact array index when applicable.
    std::optional<std::string> fieldName;             ///< Field involved in the validation failure.
    std::optional<std::string> artifactId;            ///< Artifact id when available.
    std::optional<std::filesystem::path> resolvedPath; ///< Resolved artifact path when applicable.
};

/// Result of runtime startup validation for an artifact manifest.
struct ArtifactStartupValidationResult
{
    ArtifactManifest manifest;                         ///< Parsed manifest returned by the loader.
    std::filesystem::path artifactRoot;                ///< Root used for relative artifact file policy.
    std::vector<ArtifactStartupValidationError> errors; ///< Loader and startup policy failures.

    /// Return true when startup can accept the artifact manifest.
    [[nodiscard]] bool Succeeded() const noexcept { return errors.empty(); }
};

/// Applies runtime startup acceptance policy to artifact manifests.
class ArtifactStartupValidator
{
public:
    /// Validate a manifest for runtime startup consumption.
    [[nodiscard]] static ArtifactStartupValidationResult ValidateManifestForStartup(
        const std::filesystem::path& manifestPath);
};

} // namespace SagaEngine::Artifacts
