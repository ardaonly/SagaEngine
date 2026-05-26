/// @file AssetManifestWriter.cpp
/// @brief Implements Runtime-compatible asset manifest writing.

#include "SagaAssetPipeline/Assets/AssetManifestWriter.hpp"

#include <nlohmann/json.hpp>

#include <array>
#include <cstdint>
#include <fstream>
#include <string_view>
#include <system_error>
#include <unordered_set>
#include <utility>

namespace SagaAssetPipeline {
namespace {

constexpr std::uint32_t kSchemaVersion = 1;
constexpr std::array<std::string_view, 6> kSupportedAssetKinds = {
    "mesh",
    "texture",
    "shader",
    "audio",
    "animation",
    "material",
};

[[nodiscard]] bool IsSupportedAssetKind(std::string_view kind) noexcept
{
    for (std::string_view supportedKind : kSupportedAssetKinds)
    {
        if (kind == supportedKind)
        {
            return true;
        }
    }

    return false;
}

[[nodiscard]] bool IsSafeRelativePath(const std::filesystem::path& path)
{
    if (path.empty() || path.is_absolute())
    {
        return false;
    }

    const std::filesystem::path normalized = path.lexically_normal();
    if (normalized.empty() || normalized == ".")
    {
        return false;
    }

    for (const std::filesystem::path& part : normalized)
    {
        if (part == "..")
        {
            return false;
        }
    }

    return true;
}

[[nodiscard]] AssetManifestWriteDiagnostic MakeDiagnostic(
    std::string diagnosticId,
    std::string message,
    const std::filesystem::path& outputPath,
    std::optional<std::size_t> assetIndex = std::nullopt,
    std::optional<std::string> fieldName = std::nullopt,
    std::string assetId = {},
    std::string assetKind = {},
    std::filesystem::path assetPath = {})
{
    AssetManifestWriteDiagnostic diagnostic;
    diagnostic.diagnosticId = std::move(diagnosticId);
    diagnostic.message = std::move(message);
    diagnostic.outputPath = outputPath;
    diagnostic.assetIndex = assetIndex;
    diagnostic.fieldName = std::move(fieldName);
    diagnostic.assetId = std::move(assetId);
    diagnostic.assetKind = std::move(assetKind);
    diagnostic.assetPath = std::move(assetPath);
    return diagnostic;
}

[[nodiscard]] AssetManifestWriteResult ValidateAssets(
    const std::filesystem::path& outputPath,
    const std::vector<AssetManifestWriterEntry>& assets)
{
    AssetManifestWriteResult result;
    std::unordered_set<std::string> seenAssetIds;

    for (std::size_t index = 0; index < assets.size(); ++index)
    {
        const AssetManifestWriterEntry& asset = assets[index];
        if (asset.id.empty())
        {
            result.diagnostics.push_back(MakeDiagnostic(
                AssetManifestWriterDiagnostics::InvalidAssetId,
                "Asset manifest entry id must be non-empty.",
                outputPath,
                index,
                "id"));
        }
        else if (!seenAssetIds.insert(asset.id).second)
        {
            result.diagnostics.push_back(MakeDiagnostic(
                AssetManifestWriterDiagnostics::DuplicateAssetId,
                "Asset manifest entries contain a duplicate id.",
                outputPath,
                index,
                "id",
                asset.id,
                asset.kind,
                asset.path));
        }

        if (!IsSupportedAssetKind(asset.kind))
        {
            result.diagnostics.push_back(MakeDiagnostic(
                AssetManifestWriterDiagnostics::InvalidAssetKind,
                "Asset manifest entry kind must be a Runtime-supported kind token.",
                outputPath,
                index,
                "kind",
                asset.id,
                asset.kind,
                asset.path));
        }

        if (!IsSafeRelativePath(asset.path))
        {
            result.diagnostics.push_back(MakeDiagnostic(
                AssetManifestWriterDiagnostics::InvalidAssetPath,
                "Asset manifest entry path must be a non-escaping relative path.",
                outputPath,
                index,
                "path",
                asset.id,
                asset.kind,
                asset.path));
        }

        for (std::size_t dependencyIndex = 0;
             dependencyIndex < asset.dependencies.size();
             ++dependencyIndex)
        {
            if (asset.dependencies[dependencyIndex].empty())
            {
                result.diagnostics.push_back(MakeDiagnostic(
                    AssetManifestWriterDiagnostics::InvalidDependency,
                    "Asset manifest entry dependencies must be non-empty asset ids.",
                    outputPath,
                    index,
                    "dependencies",
                    asset.id,
                    asset.kind,
                    asset.path));
                break;
            }
        }
    }

    return result;
}

[[nodiscard]] nlohmann::json BuildManifestJson(
    const std::vector<AssetManifestWriterEntry>& assets)
{
    nlohmann::json root = {
        {"schemaVersion", kSchemaVersion},
        {"assets", nlohmann::json::array()},
    };

    for (const AssetManifestWriterEntry& asset : assets)
    {
        nlohmann::json assetJson = {
            {"id", asset.id},
            {"kind", asset.kind},
            {"path", asset.path},
        };

        if (asset.hash.has_value())
        {
            assetJson["hash"] = *asset.hash;
        }
        if (!asset.dependencies.empty())
        {
            assetJson["dependencies"] = asset.dependencies;
        }
        if (asset.memoryEstimateBytes.has_value())
        {
            assetJson["memoryEstimateBytes"] = *asset.memoryEstimateBytes;
        }
        if (asset.streamingGroup.has_value())
        {
            assetJson["streamingGroup"] = *asset.streamingGroup;
        }
        if (asset.platform.has_value())
        {
            assetJson["platform"] = *asset.platform;
        }
        if (asset.profile.has_value())
        {
            assetJson["profile"] = *asset.profile;
        }

        root["assets"].push_back(std::move(assetJson));
    }

    return root;
}

} // namespace

AssetManifestWriteResult AssetManifestWriter::WriteToFile(
    const std::filesystem::path& outputPath,
    const std::vector<AssetManifestWriterEntry>& assets)
{
    AssetManifestWriteResult result = ValidateAssets(outputPath, assets);
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
                AssetManifestWriterDiagnostics::OutputDirectoryFailed,
                "Asset manifest output directory could not be created.",
                outputPath));
            return result;
        }
    }

    std::ofstream output(outputPath);
    if (!output.is_open())
    {
        result.diagnostics.push_back(MakeDiagnostic(
            AssetManifestWriterDiagnostics::OutputOpenFailed,
            "Asset manifest output file could not be opened.",
            outputPath));
        return result;
    }

    output << BuildManifestJson(assets).dump(2) << '\n';
    if (!output.good())
    {
        result.diagnostics.push_back(MakeDiagnostic(
            AssetManifestWriterDiagnostics::OutputWriteFailed,
            "Asset manifest output file could not be written.",
            outputPath));
        return result;
    }

    result.writtenAssetCount = assets.size();
    return result;
}

} // namespace SagaAssetPipeline
