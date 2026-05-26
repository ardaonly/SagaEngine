/// @file AssetIdentityManifestWriter.cpp
/// @brief Implements Runtime-compatible asset identity manifest writing.

#include "SagaAssetPipeline/Identity/AssetIdentityManifestWriter.hpp"

#include <nlohmann/json.hpp>

#include <cstdint>
#include <fstream>
#include <system_error>
#include <unordered_map>
#include <utility>

namespace SagaAssetPipeline {
namespace {

constexpr std::uint32_t kSchemaVersion = 1;

[[nodiscard]] AssetIdentityManifestWriteDiagnostic MakeDiagnostic(
    std::string diagnosticId,
    std::string message,
    const std::filesystem::path& outputPath,
    std::optional<std::size_t> mappingIndex = std::nullopt,
    std::optional<std::string> fieldName = std::nullopt,
    std::string assetKey = {},
    AssetId assetId = kInvalidAssetId)
{
    AssetIdentityManifestWriteDiagnostic diagnostic;
    diagnostic.diagnosticId = std::move(diagnosticId);
    diagnostic.message = std::move(message);
    diagnostic.outputPath = outputPath;
    diagnostic.mappingIndex = mappingIndex;
    diagnostic.fieldName = std::move(fieldName);
    diagnostic.assetKey = std::move(assetKey);
    diagnostic.assetId = assetId;
    return diagnostic;
}

[[nodiscard]] AssetIdentityManifestWriteResult ValidateMappings(
    const std::filesystem::path& outputPath,
    const std::vector<AssetIdentityMapping>& mappings)
{
    AssetIdentityManifestWriteResult result;
    std::unordered_map<std::string, std::size_t> seenKeys;
    std::unordered_map<AssetId, std::string> seenIds;

    for (std::size_t index = 0; index < mappings.size(); ++index)
    {
        const AssetIdentityMapping& mapping = mappings[index];
        if (mapping.assetKey.empty())
        {
            result.diagnostics.push_back(MakeDiagnostic(
                AssetIdentityManifestWriterDiagnostics::InvalidAssetKey,
                "Asset identity mapping assetKey must be non-empty.",
                outputPath,
                index,
                "assetKey"));
            continue;
        }

        if (mapping.assetId == kInvalidAssetId)
        {
            result.diagnostics.push_back(MakeDiagnostic(
                AssetIdentityManifestWriterDiagnostics::InvalidAssetId,
                "Asset identity mapping assetId must be greater than zero.",
                outputPath,
                index,
                "assetId",
                mapping.assetKey,
                mapping.assetId));
            continue;
        }

        if (seenKeys.find(mapping.assetKey) != seenKeys.end())
        {
            result.diagnostics.push_back(MakeDiagnostic(
                AssetIdentityManifestWriterDiagnostics::DuplicateAssetKey,
                "Asset identity mappings contain a duplicate AssetKey.",
                outputPath,
                index,
                "assetKey",
                mapping.assetKey,
                mapping.assetId));
        }
        else
        {
            seenKeys.emplace(mapping.assetKey, index);
        }

        const auto existingId = seenIds.find(mapping.assetId);
        if (existingId != seenIds.end() && existingId->second != mapping.assetKey)
        {
            result.diagnostics.push_back(MakeDiagnostic(
                AssetIdentityManifestWriterDiagnostics::DuplicateAssetId,
                "Asset identity mappings contain a duplicate AssetId.",
                outputPath,
                index,
                "assetId",
                mapping.assetKey,
                mapping.assetId));
        }
        else
        {
            seenIds.emplace(mapping.assetId, mapping.assetKey);
        }
    }

    return result;
}

[[nodiscard]] nlohmann::json BuildManifestJson(
    const std::vector<AssetIdentityMapping>& mappings)
{
    nlohmann::json root = {
        {"schemaVersion", kSchemaVersion},
        {"mappings", nlohmann::json::array()},
    };

    for (const AssetIdentityMapping& mapping : mappings)
    {
        root["mappings"].push_back({
            {"assetKey", mapping.assetKey},
            {"assetId", mapping.assetId},
        });
    }

    return root;
}

} // namespace

AssetIdentityManifestWriteResult AssetIdentityManifestWriter::WriteToFile(
    const std::filesystem::path& outputPath,
    const std::vector<AssetIdentityMapping>& mappings)
{
    AssetIdentityManifestWriteResult result =
        ValidateMappings(outputPath, mappings);
    if (!result.Succeeded())
    {
        return result;
    }

    const std::filesystem::path parentPath = outputPath.parent_path();
    if (!parentPath.empty())
    {
        std::error_code createError;
        std::filesystem::create_directories(parentPath, createError);
        if (createError)
        {
            result.diagnostics.push_back(MakeDiagnostic(
                AssetIdentityManifestWriterDiagnostics::OutputDirectoryFailed,
                "Asset identity manifest output directory could not be created.",
                outputPath));
            return result;
        }
    }

    std::ofstream output(outputPath);
    if (!output.is_open())
    {
        result.diagnostics.push_back(MakeDiagnostic(
            AssetIdentityManifestWriterDiagnostics::OutputOpenFailed,
            "Asset identity manifest output file could not be opened.",
            outputPath));
        return result;
    }

    output << BuildManifestJson(mappings).dump(2) << '\n';
    if (!output.good())
    {
        result.diagnostics.push_back(MakeDiagnostic(
            AssetIdentityManifestWriterDiagnostics::OutputWriteFailed,
            "Asset identity manifest output file could not be written.",
            outputPath));
        return result;
    }

    result.writtenMappingCount = mappings.size();
    return result;
}

} // namespace SagaAssetPipeline
