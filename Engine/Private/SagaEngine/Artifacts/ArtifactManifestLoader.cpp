/// @file ArtifactManifestLoader.cpp
/// @brief Implements runtime artifact manifest JSON loading and validation.

#include <SagaEngine/Artifacts/ArtifactManifestLoader.hpp>

#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <string>
#include <utility>

namespace SagaEngine::Artifacts
{

namespace
{

constexpr std::uint32_t kSupportedSchemaVersion = 1;

/// Build a structured loader diagnostic with optional artifact and field context.
[[nodiscard]] ArtifactManifestError MakeError(
    std::string diagnosticId,
    std::string message,
    const std::filesystem::path& manifestPath,
    std::optional<std::size_t> artifactIndex = std::nullopt,
    std::optional<std::string> fieldName = std::nullopt)
{
    ArtifactManifestError error;
    error.diagnosticId = std::move(diagnosticId);
    error.message = std::move(message);
    error.manifestPath = manifestPath;
    error.artifactIndex = artifactIndex;
    error.fieldName = std::move(fieldName);
    return error;
}

/// Resolve artifact file references against explicit options or the manifest folder.
[[nodiscard]] std::filesystem::path ResolveArtifactPath(
    const std::filesystem::path& manifestPath,
    const ArtifactManifestLoadOptions& options,
    const std::filesystem::path& artifactPath)
{
    if (artifactPath.is_absolute())
    {
        return artifactPath;
    }

    const std::filesystem::path basePath =
        options.artifactBaseDirectory.empty()
            ? manifestPath.parent_path()
            : options.artifactBaseDirectory;

    if (basePath.empty())
    {
        return artifactPath.lexically_normal();
    }

    return (basePath / artifactPath).lexically_normal();
}

/// Validate a required string field and preserve missing versus malformed diagnostics.
[[nodiscard]] bool ValidateRequiredStringField(
    const nlohmann::json& object,
    std::string_view field,
    const std::filesystem::path& manifestPath,
    std::size_t artifactIndex,
    ArtifactManifestLoadResult& result)
{
    const auto iterator = object.find(field);
    if (iterator == object.end())
    {
        result.errors.push_back(MakeError(
            ArtifactManifestDiagnostics::MissingField,
            "Artifact entry is missing a required field.",
            manifestPath,
            artifactIndex,
            std::string(field)));
        return false;
    }

    if (!iterator->is_string())
    {
        result.errors.push_back(MakeError(
            ArtifactManifestDiagnostics::InvalidField,
            "Artifact entry field must be a string.",
            manifestPath,
            artifactIndex,
            std::string(field)));
        return false;
    }

    return true;
}

} // namespace

std::optional<ArtifactKind> TryParseArtifactKind(std::string_view value) noexcept
{
    if (value == "graph")
    {
        return ArtifactKind::Graph;
    }
    if (value == "script")
    {
        return ArtifactKind::Script;
    }
    if (value == "asset")
    {
        return ArtifactKind::Asset;
    }
    if (value == "data")
    {
        return ArtifactKind::Data;
    }
    if (value == "manifest")
    {
        return ArtifactKind::Manifest;
    }
    if (value == "diagnostic_report")
    {
        return ArtifactKind::DiagnosticReport;
    }

    return std::nullopt;
}

std::string_view ToString(ArtifactKind kind) noexcept
{
    switch (kind)
    {
    case ArtifactKind::Graph:
        return "graph";
    case ArtifactKind::Script:
        return "script";
    case ArtifactKind::Asset:
        return "asset";
    case ArtifactKind::Data:
        return "data";
    case ArtifactKind::Manifest:
        return "manifest";
    case ArtifactKind::DiagnosticReport:
        return "diagnostic_report";
    }

    return "";
}

ArtifactManifestLoadResult ArtifactManifestLoader::LoadFromFile(
    const std::filesystem::path& manifestPath)
{
    return LoadFromFile(manifestPath, ArtifactManifestLoadOptions{});
}

ArtifactManifestLoadResult ArtifactManifestLoader::LoadFromFile(
    const std::filesystem::path& manifestPath,
    const ArtifactManifestLoadOptions& options)
{
    ArtifactManifestLoadResult result;

    std::ifstream input(manifestPath);
    if (!input.is_open())
    {
        result.errors.push_back(MakeError(
            ArtifactManifestDiagnostics::ManifestMissing,
            "Artifact manifest file could not be opened.",
            manifestPath));
        return result;
    }

    nlohmann::json root;
    try
    {
        input >> root;
    }
    catch (const nlohmann::json::exception& exception)
    {
        result.errors.push_back(MakeError(
            ArtifactManifestDiagnostics::ParseFailed,
            std::string("Artifact manifest JSON parse failed: ") + exception.what(),
            manifestPath));
        return result;
    }

    if (!root.is_object())
    {
        result.errors.push_back(MakeError(
            ArtifactManifestDiagnostics::ParseFailed,
            "Artifact manifest root must be a JSON object.",
            manifestPath));
        return result;
    }

    const auto schemaVersionIterator = root.find("schemaVersion");
    if (schemaVersionIterator == root.end())
    {
        result.errors.push_back(MakeError(
            ArtifactManifestDiagnostics::MissingField,
            "Artifact manifest must contain schemaVersion.",
            manifestPath,
            std::nullopt,
            "schemaVersion"));
        return result;
    }

    if (!schemaVersionIterator->is_number_integer() &&
        !schemaVersionIterator->is_number_unsigned())
    {
        result.errors.push_back(MakeError(
            ArtifactManifestDiagnostics::InvalidField,
            "Artifact manifest schemaVersion must be an integer.",
            manifestPath,
            std::nullopt,
            "schemaVersion"));
        return result;
    }

    bool supportedSchemaVersion = false;
    if (schemaVersionIterator->is_number_unsigned())
    {
        supportedSchemaVersion =
            schemaVersionIterator->get<std::uint64_t>() == kSupportedSchemaVersion;
    }
    else
    {
        supportedSchemaVersion =
            schemaVersionIterator->get<std::int64_t>() ==
            static_cast<std::int64_t>(kSupportedSchemaVersion);
    }

    if (!supportedSchemaVersion)
    {
        result.errors.push_back(MakeError(
            ArtifactManifestDiagnostics::UnsupportedVersion,
            "Artifact manifest schemaVersion is not supported.",
            manifestPath,
            std::nullopt,
            "schemaVersion"));
        return result;
    }
    result.manifest.schemaVersion = kSupportedSchemaVersion;

    const auto artifactsIterator = root.find("artifacts");
    if (artifactsIterator == root.end())
    {
        result.errors.push_back(MakeError(
            ArtifactManifestDiagnostics::MissingField,
            "Artifact manifest must contain artifacts.",
            manifestPath,
            std::nullopt,
            "artifacts"));
        return result;
    }

    if (!artifactsIterator->is_array())
    {
        result.errors.push_back(MakeError(
            ArtifactManifestDiagnostics::InvalidField,
            "Artifact manifest artifacts field must be an array.",
            manifestPath,
            std::nullopt,
            "artifacts"));
        return result;
    }

    for (std::size_t index = 0; index < artifactsIterator->size(); ++index)
    {
        const auto& item = (*artifactsIterator)[index];
        if (!item.is_object())
        {
            result.errors.push_back(MakeError(
                ArtifactManifestDiagnostics::InvalidField,
                "Artifact entry must be a JSON object.",
                manifestPath,
                index));
            continue;
        }

        bool valid = true;
        for (std::string_view field : {"id", "kind", "path"})
        {
            if (!ValidateRequiredStringField(item, field, manifestPath, index, result))
            {
                valid = false;
            }
        }

        if (!valid)
        {
            continue;
        }

        if (item.contains("hash") && !item["hash"].is_string())
        {
            result.errors.push_back(MakeError(
                ArtifactManifestDiagnostics::InvalidField,
                "Artifact hash must be a string when present.",
                manifestPath,
                index,
                "hash"));
            continue;
        }

        const std::string kindText = item["kind"].get<std::string>();
        const auto kind = TryParseArtifactKind(kindText);
        if (!kind.has_value())
        {
            result.errors.push_back(MakeError(
                ArtifactManifestDiagnostics::UnknownKind,
                "Artifact entry uses an unknown kind.",
                manifestPath,
                index,
                "kind"));
            continue;
        }

        ArtifactRef artifact;
        artifact.id = item["id"].get<std::string>();
        artifact.kind = *kind;
        artifact.path = item["path"].get<std::string>();
        if (item.contains("hash"))
        {
            artifact.hash = item["hash"].get<std::string>();
        }

        if (options.validateArtifactFiles)
        {
            const auto resolvedPath =
                ResolveArtifactPath(manifestPath, options, artifact.path);
            if (!std::filesystem::exists(resolvedPath))
            {
                result.errors.push_back(MakeError(
                    ArtifactManifestDiagnostics::FileMissing,
                    "Artifact entry references a file that does not exist.",
                    manifestPath,
                    index,
                    "path"));
            }
        }

        result.manifest.artifacts.push_back(std::move(artifact));
    }

    return result;
}

} // namespace SagaEngine::Artifacts
