/// @file AssetIdentityManifestLoader.cpp
/// @brief Implements package AssetKey to runtime AssetId manifest loading.

#include "SagaEngine/Resources/AssetIdentityManifest.h"

#include <nlohmann/json.hpp>

#include <fstream>
#include <string>
#include <unordered_map>
#include <utility>

namespace SagaEngine::Resources {
namespace {

constexpr std::uint32_t kSupportedSchemaVersion = 1;

/// Build a structured loader diagnostic with optional mapping context.
[[nodiscard]] AssetIdentityManifestError MakeError(
    std::string diagnosticId,
    std::string message,
    const std::filesystem::path& manifestPath,
    std::optional<std::size_t> mappingIndex = std::nullopt,
    std::optional<std::string> fieldName = std::nullopt,
    AssetKey assetKey = {},
    AssetId assetId = kInvalidAssetId)
{
    AssetIdentityManifestError error;
    error.diagnosticId = std::move(diagnosticId);
    error.message = std::move(message);
    error.manifestPath = manifestPath;
    error.mappingIndex = mappingIndex;
    error.fieldName = std::move(fieldName);
    error.assetKey = std::move(assetKey);
    error.assetId = assetId;
    return error;
}

/// Parse a manifest AssetId value while preserving zero as invalid.
[[nodiscard]] bool TryParseAssetId(
    const nlohmann::json& value,
    AssetId& outAssetId)
{
    if (value.is_number_unsigned())
    {
        outAssetId = value.get<AssetId>();
        return outAssetId != kInvalidAssetId;
    }

    if (value.is_number_integer())
    {
        const auto signedValue = value.get<std::int64_t>();
        if (signedValue <= 0)
        {
            outAssetId = kInvalidAssetId;
            return false;
        }

        outAssetId = static_cast<AssetId>(signedValue);
        return true;
    }

    outAssetId = kInvalidAssetId;
    return false;
}

} // namespace

AssetIdentityManifestLoadResult AssetIdentityManifestLoader::LoadFromFile(
    const std::filesystem::path& manifestPath)
{
    AssetIdentityManifestLoadResult result;

    std::ifstream input(manifestPath);
    if (!input.is_open())
    {
        result.errors.push_back(MakeError(
            AssetIdentityManifestDiagnostics::ManifestMissing,
            "Asset identity manifest file could not be opened.",
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
            AssetIdentityManifestDiagnostics::ParseFailed,
            std::string("Asset identity manifest JSON parse failed: ") +
                exception.what(),
            manifestPath));
        return result;
    }

    if (!root.is_object())
    {
        result.errors.push_back(MakeError(
            AssetIdentityManifestDiagnostics::ParseFailed,
            "Asset identity manifest root must be a JSON object.",
            manifestPath));
        return result;
    }

    const auto schemaVersionIterator = root.find("schemaVersion");
    if (schemaVersionIterator == root.end())
    {
        result.errors.push_back(MakeError(
            AssetIdentityManifestDiagnostics::MissingField,
            "Asset identity manifest must contain schemaVersion.",
            manifestPath,
            std::nullopt,
            "schemaVersion"));
        return result;
    }

    if (!schemaVersionIterator->is_number_integer() &&
        !schemaVersionIterator->is_number_unsigned())
    {
        result.errors.push_back(MakeError(
            AssetIdentityManifestDiagnostics::InvalidField,
            "Asset identity manifest schemaVersion must be an integer.",
            manifestPath,
            std::nullopt,
            "schemaVersion"));
        return result;
    }

    const bool supportedSchemaVersion =
        schemaVersionIterator->is_number_unsigned()
            ? schemaVersionIterator->get<std::uint64_t>() ==
                  kSupportedSchemaVersion
            : schemaVersionIterator->get<std::int64_t>() ==
                  static_cast<std::int64_t>(kSupportedSchemaVersion);

    if (!supportedSchemaVersion)
    {
        result.errors.push_back(MakeError(
            AssetIdentityManifestDiagnostics::UnsupportedVersion,
            "Asset identity manifest schemaVersion is not supported.",
            manifestPath,
            std::nullopt,
            "schemaVersion"));
        return result;
    }
    result.manifest.schemaVersion = kSupportedSchemaVersion;

    const auto mappingsIterator = root.find("mappings");
    if (mappingsIterator == root.end())
    {
        result.errors.push_back(MakeError(
            AssetIdentityManifestDiagnostics::MissingField,
            "Asset identity manifest must contain mappings.",
            manifestPath,
            std::nullopt,
            "mappings"));
        return result;
    }

    if (!mappingsIterator->is_array())
    {
        result.errors.push_back(MakeError(
            AssetIdentityManifestDiagnostics::InvalidField,
            "Asset identity manifest mappings field must be an array.",
            manifestPath,
            std::nullopt,
            "mappings"));
        return result;
    }

    std::unordered_map<AssetKey, std::size_t> seenKeys;
    std::unordered_map<AssetId, AssetKey> seenIds;

    for (std::size_t index = 0; index < mappingsIterator->size(); ++index)
    {
        const auto& item = (*mappingsIterator)[index];
        if (!item.is_object())
        {
            result.errors.push_back(MakeError(
                AssetIdentityManifestDiagnostics::InvalidField,
                "Asset identity mapping must be a JSON object.",
                manifestPath,
                index));
            continue;
        }

        bool valid = true;
        const auto keyIterator = item.find("assetKey");
        if (keyIterator == item.end())
        {
            result.errors.push_back(MakeError(
                AssetIdentityManifestDiagnostics::MissingField,
                "Asset identity mapping is missing assetKey.",
                manifestPath,
                index,
                "assetKey"));
            valid = false;
        }
        else if (!keyIterator->is_string() ||
                 keyIterator->get<std::string>().empty())
        {
            result.errors.push_back(MakeError(
                AssetIdentityManifestDiagnostics::InvalidField,
                "Asset identity mapping assetKey must be a non-empty string.",
                manifestPath,
                index,
                "assetKey"));
            valid = false;
        }

        const auto idIterator = item.find("assetId");
        AssetId assetId = kInvalidAssetId;
        if (idIterator == item.end())
        {
            result.errors.push_back(MakeError(
                AssetIdentityManifestDiagnostics::MissingField,
                "Asset identity mapping is missing assetId.",
                manifestPath,
                index,
                "assetId"));
            valid = false;
        }
        else if (!TryParseAssetId(*idIterator, assetId))
        {
            result.errors.push_back(MakeError(
                AssetIdentityManifestDiagnostics::InvalidField,
                "Asset identity mapping assetId must be a numeric value greater than zero.",
                manifestPath,
                index,
                "assetId",
                {},
                assetId));
            valid = false;
        }

        if (!valid)
        {
            continue;
        }

        const AssetKey assetKey = keyIterator->get<std::string>();
        if (seenKeys.find(assetKey) != seenKeys.end())
        {
            result.errors.push_back(MakeError(
                AssetIdentityManifestDiagnostics::DuplicateAssetKey,
                "Asset identity manifest contains the same AssetKey more than once.",
                manifestPath,
                index,
                "assetKey",
                assetKey,
                assetId));
            continue;
        }
        seenKeys.emplace(assetKey, index);

        const auto existingId = seenIds.find(assetId);
        if (existingId != seenIds.end() && existingId->second != assetKey)
        {
            result.errors.push_back(MakeError(
                AssetIdentityManifestDiagnostics::DuplicateAssetId,
                "Asset identity manifest maps multiple AssetKeys to the same AssetId.",
                manifestPath,
                index,
                "assetId",
                assetKey,
                assetId));
            continue;
        }
        seenIds.emplace(assetId, assetKey);

        AssetIdentityMapping mapping;
        mapping.assetKey = assetKey;
        mapping.assetId = assetId;
        result.manifest.mappings.push_back(std::move(mapping));
    }

    return result;
}

StaticAssetIdResolver AssetIdentityManifestLoader::BuildResolver(
    const AssetIdentityManifest& manifest)
{
    StaticAssetIdResolver resolver;
    for (const AssetIdentityMapping& mapping : manifest.mappings)
    {
        resolver.AddMapping(mapping.assetKey, mapping.assetId);
    }
    return resolver;
}

} // namespace SagaEngine::Resources
