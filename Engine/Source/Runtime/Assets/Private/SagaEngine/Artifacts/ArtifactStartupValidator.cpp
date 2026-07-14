/// @file ArtifactStartupValidator.cpp
/// @brief Implements runtime startup acceptance policy for artifact manifests.

#include <SagaEngine/Artifacts/ArtifactStartupValidator.hpp>

#include <string>
#include <unordered_set>
#include <utility>

namespace SagaEngine::Artifacts
{

namespace
{

/// Build a structured startup policy diagnostic.
[[nodiscard]] ArtifactStartupValidationError MakeError(
    std::string diagnosticId,
    std::string message,
    const std::filesystem::path& manifestPath,
    std::optional<std::size_t> artifactIndex = std::nullopt,
    std::optional<std::string> fieldName = std::nullopt,
    std::optional<std::string> artifactId = std::nullopt,
    std::optional<std::filesystem::path> resolvedPath = std::nullopt)
{
    ArtifactStartupValidationError error;
    error.diagnosticId = std::move(diagnosticId);
    error.message = std::move(message);
    error.manifestPath = manifestPath;
    error.artifactIndex = artifactIndex;
    error.fieldName = std::move(fieldName);
    error.artifactId = std::move(artifactId);
    error.resolvedPath = std::move(resolvedPath);
    return error;
}

/// Convert loader diagnostics into startup validation diagnostics.
[[nodiscard]] ArtifactStartupValidationError FromLoaderError(
    const ArtifactManifestError& loaderError)
{
    ArtifactStartupValidationError error;
    error.diagnosticId = loaderError.diagnosticId;
    error.message = loaderError.message;
    error.manifestPath = loaderError.manifestPath;
    error.artifactIndex = loaderError.artifactIndex;
    error.fieldName = loaderError.fieldName;
    return error;
}

/// Return the startup artifact root derived from the manifest path.
[[nodiscard]] std::filesystem::path ResolveArtifactRoot(
    const std::filesystem::path& manifestPath)
{
    const auto parent = manifestPath.parent_path();
    if (parent.empty())
    {
        return std::filesystem::path(".").lexically_normal();
    }

    return parent.lexically_normal();
}

/// Resolve a manifest artifact path against the startup artifact root.
[[nodiscard]] std::filesystem::path ResolveArtifactPath(
    const std::filesystem::path& artifactRoot,
    const std::filesystem::path& artifactPath)
{
    return (artifactRoot / artifactPath).lexically_normal();
}

/// Check whether a resolved path escapes the startup artifact root.
[[nodiscard]] bool PathEscapesRoot(
    const std::filesystem::path& artifactRoot,
    const std::filesystem::path& resolvedPath)
{
    const auto absoluteRoot = std::filesystem::absolute(artifactRoot).lexically_normal();
    const auto absolutePath = std::filesystem::absolute(resolvedPath).lexically_normal();
    const auto relativePath = absolutePath.lexically_relative(absoluteRoot);

    if (relativePath.empty())
    {
        return false;
    }

    const auto firstComponent = *relativePath.begin();
    return firstComponent == ".." || firstComponent.empty();
}

} // namespace

ArtifactStartupValidationResult ArtifactStartupValidator::ValidateManifestForStartup(
    const std::filesystem::path& manifestPath)
{
    ArtifactStartupValidationResult result;
    result.artifactRoot = ResolveArtifactRoot(manifestPath);

    const auto loadResult = ArtifactManifestLoader::LoadFromFile(manifestPath);
    result.manifest = loadResult.manifest;
    if (!loadResult.Succeeded())
    {
        for (const auto& error : loadResult.errors)
        {
            result.errors.push_back(FromLoaderError(error));
        }
        return result;
    }

    std::unordered_set<std::string> artifactIds;
    for (std::size_t index = 0; index < result.manifest.artifacts.size(); ++index)
    {
        const auto& artifact = result.manifest.artifacts[index];

        if (artifact.id.empty())
        {
            result.errors.push_back(MakeError(
                ArtifactManifestDiagnostics::InvalidField,
                "Artifact id must not be empty for runtime startup.",
                manifestPath,
                index,
                "id",
                artifact.id));
        }
        else if (!artifactIds.insert(artifact.id).second)
        {
            result.errors.push_back(MakeError(
                ArtifactManifestDiagnostics::DuplicateId,
                "Artifact id must be unique for runtime startup.",
                manifestPath,
                index,
                "id",
                artifact.id));
        }

        if (artifact.path.empty())
        {
            result.errors.push_back(MakeError(
                ArtifactManifestDiagnostics::InvalidField,
                "Artifact path must not be empty for runtime startup.",
                manifestPath,
                index,
                "path",
                artifact.id));
            continue;
        }

        const std::filesystem::path artifactPath(artifact.path);
        if (artifactPath.is_absolute())
        {
            result.errors.push_back(MakeError(
                ArtifactManifestDiagnostics::InvalidField,
                "Artifact path must be relative to the startup artifact root.",
                manifestPath,
                index,
                "path",
                artifact.id,
                artifactPath));
            continue;
        }

        const auto resolvedPath = ResolveArtifactPath(result.artifactRoot, artifactPath);
        if (PathEscapesRoot(result.artifactRoot, resolvedPath))
        {
            result.errors.push_back(MakeError(
                ArtifactManifestDiagnostics::InvalidField,
                "Artifact path must not escape the startup artifact root.",
                manifestPath,
                index,
                "path",
                artifact.id,
                resolvedPath));
            continue;
        }

        if (!std::filesystem::exists(resolvedPath))
        {
            result.errors.push_back(MakeError(
                ArtifactManifestDiagnostics::FileMissing,
                "Artifact file referenced by the startup manifest does not exist.",
                manifestPath,
                index,
                "path",
                artifact.id,
                resolvedPath));
        }
    }

    return result;
}

} // namespace SagaEngine::Artifacts
