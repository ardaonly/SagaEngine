/// @file PackageManifestWriter.cpp
/// @brief Implements Runtime-compatible package manifest writing.

#include "SagaAssetPipeline/Packages/PackageManifestWriter.hpp"

#include <nlohmann/json.hpp>

#include <cstdint>
#include <fstream>
#include <string_view>
#include <system_error>
#include <unordered_set>
#include <utility>

namespace SagaAssetPipeline {
namespace {

constexpr std::uint32_t kSchemaVersion = 1;

[[nodiscard]] std::string_view ToManifestToken(PackageManifestKind kind) noexcept
{
    switch (kind)
    {
    case PackageManifestKind::Client:
        return "client";
    case PackageManifestKind::Server:
        return "server";
    case PackageManifestKind::Invalid:
        break;
    }

    return "";
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

[[nodiscard]] PackageManifestWriteDiagnostic MakeDiagnostic(
    std::string diagnosticId,
    std::string message,
    const std::filesystem::path& outputPath,
    std::optional<std::size_t> referenceIndex = std::nullopt,
    std::optional<std::string> fieldName = std::nullopt,
    std::string referenceId = {},
    std::filesystem::path referencePath = {})
{
    PackageManifestWriteDiagnostic diagnostic;
    diagnostic.diagnosticId = std::move(diagnosticId);
    diagnostic.message = std::move(message);
    diagnostic.outputPath = outputPath;
    diagnostic.referenceIndex = referenceIndex;
    diagnostic.fieldName = std::move(fieldName);
    diagnostic.referenceId = std::move(referenceId);
    diagnostic.referencePath = std::move(referencePath);
    return diagnostic;
}

void ValidateManifestRefs(
    const std::filesystem::path& outputPath,
    const std::vector<PackageManifestRefWriteInput>& refs,
    std::string_view fieldName,
    PackageManifestWriteResult& result)
{
    std::unordered_set<std::string> seenIds;

    for (std::size_t index = 0; index < refs.size(); ++index)
    {
        const PackageManifestRefWriteInput& ref = refs[index];
        if (ref.id.empty())
        {
            result.diagnostics.push_back(MakeDiagnostic(
                PackageManifestWriterDiagnostics::InvalidManifestRefId,
                "Package manifest reference id must be non-empty.",
                outputPath,
                index,
                std::string(fieldName),
                ref.id,
                ref.path));
        }
        else if (!seenIds.insert(ref.id).second)
        {
            result.diagnostics.push_back(MakeDiagnostic(
                PackageManifestWriterDiagnostics::DuplicateManifestRefId,
                "Package manifest reference ids must be unique within each reference array.",
                outputPath,
                index,
                std::string(fieldName),
                ref.id,
                ref.path));
        }

        if (!IsSafeRelativePath(ref.path))
        {
            result.diagnostics.push_back(MakeDiagnostic(
                PackageManifestWriterDiagnostics::InvalidManifestRefPath,
                "Package manifest reference path must be a non-escaping relative path.",
                outputPath,
                index,
                std::string(fieldName),
                ref.id,
                ref.path));
        }
    }
}

[[nodiscard]] PackageManifestWriteResult ValidateInput(
    const std::filesystem::path& outputPath,
    const PackageManifestWriteInput& input)
{
    PackageManifestWriteResult result;

    if (input.packageId.empty())
    {
        result.diagnostics.push_back(MakeDiagnostic(
            PackageManifestWriterDiagnostics::InvalidPackageId,
            "Package manifest packageId must be non-empty.",
            outputPath,
            std::nullopt,
            "packageId"));
    }

    if (ToManifestToken(input.packageKind).empty())
    {
        result.diagnostics.push_back(MakeDiagnostic(
            PackageManifestWriterDiagnostics::InvalidPackageKind,
            "Package manifest packageKind must be client or server.",
            outputPath,
            std::nullopt,
            "packageKind"));
    }

    if (input.buildProfile.empty())
    {
        result.diagnostics.push_back(MakeDiagnostic(
            PackageManifestWriterDiagnostics::InvalidBuildProfile,
            "Package manifest buildProfile must be non-empty.",
            outputPath,
            std::nullopt,
            "buildProfile"));
    }

    if (input.targetPlatform.empty())
    {
        result.diagnostics.push_back(MakeDiagnostic(
            PackageManifestWriterDiagnostics::InvalidTargetPlatform,
            "Package manifest targetPlatform must be non-empty.",
            outputPath,
            std::nullopt,
            "targetPlatform"));
    }

    if (input.runtimeCompatibilityVersion.empty())
    {
        result.diagnostics.push_back(MakeDiagnostic(
            PackageManifestWriterDiagnostics::InvalidRuntimeCompatibilityVersion,
            "Package manifest runtimeCompatibilityVersion must be non-empty.",
            outputPath,
            std::nullopt,
            "runtimeCompatibilityVersion"));
    }

    if (input.assetIdentityManifest.has_value() &&
        !IsSafeRelativePath(*input.assetIdentityManifest))
    {
        result.diagnostics.push_back(MakeDiagnostic(
            PackageManifestWriterDiagnostics::InvalidAssetIdentityManifestPath,
            "Package manifest assetIdentityManifest must be a non-escaping relative path.",
            outputPath,
            std::nullopt,
            "assetIdentityManifest",
            {},
            *input.assetIdentityManifest));
    }

    ValidateManifestRefs(
        outputPath, input.assetManifests, "assetManifests", result);
    ValidateManifestRefs(
        outputPath, input.artifactManifests, "artifactManifests", result);

    return result;
}

[[nodiscard]] nlohmann::json BuildRefJson(
    const PackageManifestRefWriteInput& ref)
{
    nlohmann::json refJson = {
        {"id", ref.id},
        {"path", ref.path},
    };
    if (ref.hash.has_value())
    {
        refJson["hash"] = *ref.hash;
    }

    return refJson;
}

[[nodiscard]] nlohmann::json BuildManifestJson(
    const PackageManifestWriteInput& input)
{
    nlohmann::json root = {
        {"schemaVersion", kSchemaVersion},
        {"packageId", input.packageId},
        {"packageKind", std::string(ToManifestToken(input.packageKind))},
        {"buildProfile", input.buildProfile},
        {"targetPlatform", input.targetPlatform},
        {"runtimeCompatibilityVersion", input.runtimeCompatibilityVersion},
        {"assetManifests", nlohmann::json::array()},
        {"artifactManifests", nlohmann::json::array()},
    };

    if (input.assetIdentityManifest.has_value())
    {
        root["assetIdentityManifest"] = *input.assetIdentityManifest;
    }

    for (const PackageManifestRefWriteInput& ref : input.assetManifests)
    {
        root["assetManifests"].push_back(BuildRefJson(ref));
    }
    for (const PackageManifestRefWriteInput& ref : input.artifactManifests)
    {
        root["artifactManifests"].push_back(BuildRefJson(ref));
    }

    if (input.packageHash.has_value())
    {
        root["packageHash"] = *input.packageHash;
    }

    return root;
}

} // namespace

PackageManifestWriteResult PackageManifestWriter::WriteToFile(
    const std::filesystem::path& outputPath,
    const PackageManifestWriteInput& input)
{
    PackageManifestWriteResult result = ValidateInput(outputPath, input);
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
                PackageManifestWriterDiagnostics::OutputDirectoryFailed,
                "Package manifest output directory could not be created.",
                outputPath));
            return result;
        }
    }

    std::ofstream output(outputPath);
    if (!output.is_open())
    {
        result.diagnostics.push_back(MakeDiagnostic(
            PackageManifestWriterDiagnostics::OutputOpenFailed,
            "Package manifest output file could not be opened.",
            outputPath));
        return result;
    }

    output << BuildManifestJson(input).dump(2) << '\n';
    if (!output.good())
    {
        result.diagnostics.push_back(MakeDiagnostic(
            PackageManifestWriterDiagnostics::OutputWriteFailed,
            "Package manifest output file could not be written.",
            outputPath));
        return result;
    }

    result.writtenAssetManifestCount = input.assetManifests.size();
    result.writtenArtifactManifestCount = input.artifactManifests.size();
    return result;
}

} // namespace SagaAssetPipeline
