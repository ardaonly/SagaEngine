/// @file AssetManifestLoader.cpp
/// @brief Implements runtime asset manifest JSON loading and validation.

#include <SagaEngine/Assets/AssetManifestLoader.hpp>

#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <string>
#include <unordered_set>
#include <utility>

namespace SagaEngine::Assets
{

namespace
{

constexpr std::uint32_t kSupportedSchemaVersion = 1;

/// Build a structured loader diagnostic with optional asset and field context.
[[nodiscard]] AssetManifestError MakeError(
    std::string diagnosticId,
    std::string message,
    const std::filesystem::path& manifestPath,
    std::optional<std::size_t> assetIndex = std::nullopt,
    std::optional<std::string> fieldName = std::nullopt,
    std::optional<std::filesystem::path> resolvedPath = std::nullopt)
{
    AssetManifestError error;
    error.diagnosticId = std::move(diagnosticId);
    error.message = std::move(message);
    error.manifestPath = manifestPath;
    error.assetIndex = assetIndex;
    error.fieldName = std::move(fieldName);
    error.resolvedPath = std::move(resolvedPath);
    return error;
}

/// Return true when a manifest path is relative and cannot escape its base.
[[nodiscard]] bool IsSafeRelativePath(const std::filesystem::path& path)
{
    if (path.empty() || path.is_absolute())
    {
        return false;
    }

    const auto normalized = path.lexically_normal();
    if (normalized.empty() || normalized == ".")
    {
        return false;
    }

    for (const auto& part : normalized)
    {
        if (part == "..")
        {
            return false;
        }
    }

    return true;
}

/// Resolve cooked asset file references against explicit options or manifest folder.
[[nodiscard]] std::filesystem::path ResolveAssetPath(
    const std::filesystem::path& manifestPath,
    const AssetManifestLoadOptions& options,
    const std::filesystem::path& assetPath)
{
    const std::filesystem::path basePath =
        options.assetBaseDirectory.empty()
            ? manifestPath.parent_path()
            : options.assetBaseDirectory;

    if (basePath.empty())
    {
        return assetPath.lexically_normal();
    }

    return (basePath / assetPath).lexically_normal();
}

/// Validate a required string field and preserve missing versus malformed diagnostics.
[[nodiscard]] bool ValidateRequiredStringField(
    const nlohmann::json& object,
    std::string_view field,
    const std::filesystem::path& manifestPath,
    std::size_t assetIndex,
    AssetManifestLoadResult& result)
{
    const auto iterator = object.find(field);
    if (iterator == object.end())
    {
        result.errors.push_back(MakeError(
            AssetManifestDiagnostics::MissingField,
            "Asset entry is missing a required field.",
            manifestPath,
            assetIndex,
            std::string(field)));
        return false;
    }

    if (!iterator->is_string())
    {
        result.errors.push_back(MakeError(
            AssetManifestDiagnostics::InvalidField,
            "Asset entry field must be a string.",
            manifestPath,
            assetIndex,
            std::string(field)));
        return false;
    }

    return true;
}

/// Validate an optional string field.
[[nodiscard]] bool ValidateOptionalStringField(
    const nlohmann::json& object,
    std::string_view field,
    const std::filesystem::path& manifestPath,
    std::size_t assetIndex,
    AssetManifestLoadResult& result)
{
    const auto iterator = object.find(field);
    if (iterator == object.end())
    {
        return true;
    }

    if (!iterator->is_string())
    {
        result.errors.push_back(MakeError(
            AssetManifestDiagnostics::InvalidField,
            "Asset entry optional field must be a string.",
            manifestPath,
            assetIndex,
            std::string(field)));
        return false;
    }

    return true;
}

/// Copy an optional string field into the parsed entry.
void CopyOptionalString(
    const nlohmann::json& object,
    std::string_view field,
    std::optional<std::string>& destination)
{
    const auto iterator = object.find(field);
    if (iterator != object.end())
    {
        destination = iterator->get<std::string>();
    }
}

} // namespace

std::optional<AssetKind> TryParseAssetKind(std::string_view value) noexcept
{
    if (value == "mesh")
    {
        return AssetKind::Mesh;
    }
    if (value == "texture")
    {
        return AssetKind::Texture;
    }
    if (value == "shader")
    {
        return AssetKind::Shader;
    }
    if (value == "audio")
    {
        return AssetKind::Audio;
    }
    if (value == "animation")
    {
        return AssetKind::Animation;
    }
    if (value == "material")
    {
        return AssetKind::Material;
    }

    return std::nullopt;
}

std::string_view ToString(AssetKind kind) noexcept
{
    switch (kind)
    {
    case AssetKind::Mesh:
        return "mesh";
    case AssetKind::Texture:
        return "texture";
    case AssetKind::Shader:
        return "shader";
    case AssetKind::Audio:
        return "audio";
    case AssetKind::Animation:
        return "animation";
    case AssetKind::Material:
        return "material";
    }

    return "";
}

AssetManifestLoadResult AssetManifestLoader::LoadFromFile(
    const std::filesystem::path& manifestPath)
{
    return LoadFromFile(manifestPath, AssetManifestLoadOptions{});
}

AssetManifestLoadResult AssetManifestLoader::LoadFromFile(
    const std::filesystem::path& manifestPath,
    const AssetManifestLoadOptions& options)
{
    AssetManifestLoadResult result;

    std::ifstream input(manifestPath);
    if (!input.is_open())
    {
        result.errors.push_back(MakeError(
            AssetManifestDiagnostics::ManifestMissing,
            "Asset manifest file could not be opened.",
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
            AssetManifestDiagnostics::ParseFailed,
            std::string("Asset manifest JSON parse failed: ") + exception.what(),
            manifestPath));
        return result;
    }

    if (!root.is_object())
    {
        result.errors.push_back(MakeError(
            AssetManifestDiagnostics::ParseFailed,
            "Asset manifest root must be a JSON object.",
            manifestPath));
        return result;
    }

    const auto schemaVersionIterator = root.find("schemaVersion");
    if (schemaVersionIterator == root.end())
    {
        result.errors.push_back(MakeError(
            AssetManifestDiagnostics::MissingField,
            "Asset manifest must contain schemaVersion.",
            manifestPath,
            std::nullopt,
            "schemaVersion"));
        return result;
    }

    if (!schemaVersionIterator->is_number_integer() &&
        !schemaVersionIterator->is_number_unsigned())
    {
        result.errors.push_back(MakeError(
            AssetManifestDiagnostics::InvalidField,
            "Asset manifest schemaVersion must be an integer.",
            manifestPath,
            std::nullopt,
            "schemaVersion"));
        return result;
    }

    const bool supportedSchemaVersion =
        schemaVersionIterator->is_number_unsigned()
            ? schemaVersionIterator->get<std::uint64_t>() == kSupportedSchemaVersion
            : schemaVersionIterator->get<std::int64_t>() ==
                  static_cast<std::int64_t>(kSupportedSchemaVersion);

    if (!supportedSchemaVersion)
    {
        result.errors.push_back(MakeError(
            AssetManifestDiagnostics::UnsupportedVersion,
            "Asset manifest schemaVersion is not supported.",
            manifestPath,
            std::nullopt,
            "schemaVersion"));
        return result;
    }
    result.manifest.schemaVersion = kSupportedSchemaVersion;

    const auto assetsIterator = root.find("assets");
    if (assetsIterator == root.end())
    {
        result.errors.push_back(MakeError(
            AssetManifestDiagnostics::MissingField,
            "Asset manifest must contain assets.",
            manifestPath,
            std::nullopt,
            "assets"));
        return result;
    }

    if (!assetsIterator->is_array())
    {
        result.errors.push_back(MakeError(
            AssetManifestDiagnostics::InvalidField,
            "Asset manifest assets field must be an array.",
            manifestPath,
            std::nullopt,
            "assets"));
        return result;
    }

    std::unordered_set<std::string> assetIds;

    for (std::size_t index = 0; index < assetsIterator->size(); ++index)
    {
        const auto& item = (*assetsIterator)[index];
        if (!item.is_object())
        {
            result.errors.push_back(MakeError(
                AssetManifestDiagnostics::InvalidField,
                "Asset entry must be a JSON object.",
                manifestPath,
                index));
            continue;
        }

        bool valid = true;
        for (std::string_view field : {"id", "kind", "path"})
        {
            valid = ValidateRequiredStringField(
                item, field, manifestPath, index, result) && valid;
        }

        for (std::string_view field :
             {"hash", "streamingGroup", "platform", "profile"})
        {
            valid = ValidateOptionalStringField(
                item, field, manifestPath, index, result) && valid;
        }

        if (item.contains("memoryEstimateBytes") &&
            !item["memoryEstimateBytes"].is_number_unsigned())
        {
            result.errors.push_back(MakeError(
                AssetManifestDiagnostics::InvalidField,
                "Asset memoryEstimateBytes must be an unsigned integer when present.",
                manifestPath,
                index,
                "memoryEstimateBytes"));
            valid = false;
        }

        if (item.contains("dependencies"))
        {
            if (!item["dependencies"].is_array())
            {
                result.errors.push_back(MakeError(
                    AssetManifestDiagnostics::InvalidField,
                    "Asset dependencies must be an array when present.",
                    manifestPath,
                    index,
                    "dependencies"));
                valid = false;
            }
            else
            {
                for (const auto& dependency : item["dependencies"])
                {
                    if (!dependency.is_string())
                    {
                        result.errors.push_back(MakeError(
                            AssetManifestDiagnostics::InvalidField,
                            "Asset dependencies must contain only strings.",
                            manifestPath,
                            index,
                            "dependencies"));
                        valid = false;
                        break;
                    }
                }
            }
        }

        if (!valid)
        {
            continue;
        }

        const std::string id = item["id"].get<std::string>();
        if (id.empty() || !assetIds.insert(id).second)
        {
            result.errors.push_back(MakeError(
                id.empty() ? AssetManifestDiagnostics::InvalidField
                           : AssetManifestDiagnostics::DuplicateId,
                id.empty() ? "Asset id must not be empty."
                           : "Asset manifest contains a duplicate asset id.",
                manifestPath,
                index,
                "id"));
            continue;
        }

        const std::string path = item["path"].get<std::string>();
        if (!IsSafeRelativePath(path))
        {
            result.errors.push_back(MakeError(
                AssetManifestDiagnostics::InvalidPath,
                "Asset path must be a non-escaping relative path.",
                manifestPath,
                index,
                "path"));
            continue;
        }

        const std::string kindText = item["kind"].get<std::string>();
        const auto kind = TryParseAssetKind(kindText);
        if (!kind.has_value())
        {
            result.errors.push_back(MakeError(
                AssetManifestDiagnostics::UnknownKind,
                "Asset entry uses an unknown kind.",
                manifestPath,
                index,
                "kind"));
            continue;
        }

        if (options.validateAssetFiles)
        {
            const auto resolvedPath = ResolveAssetPath(manifestPath, options, path);
            if (!std::filesystem::exists(resolvedPath))
            {
                result.errors.push_back(MakeError(
                    AssetManifestDiagnostics::FileMissing,
                    "Asset entry references a file that does not exist.",
                    manifestPath,
                    index,
                    "path",
                    resolvedPath));
                continue;
            }
        }

        AssetManifestEntry asset;
        asset.id = id;
        asset.kind = *kind;
        asset.path = path;
        CopyOptionalString(item, "hash", asset.hash);
        CopyOptionalString(item, "streamingGroup", asset.streamingGroup);
        CopyOptionalString(item, "platform", asset.platform);
        CopyOptionalString(item, "profile", asset.profile);
        if (item.contains("memoryEstimateBytes"))
        {
            asset.memoryEstimateBytes =
                item["memoryEstimateBytes"].get<std::uint64_t>();
        }
        if (item.contains("dependencies"))
        {
            for (const auto& dependency : item["dependencies"])
            {
                asset.dependencies.push_back(dependency.get<std::string>());
            }
        }

        result.manifest.assets.push_back(std::move(asset));
    }

    return result;
}

} // namespace SagaEngine::Assets
