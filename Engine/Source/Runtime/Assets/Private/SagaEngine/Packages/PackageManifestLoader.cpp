/// @file PackageManifestLoader.cpp
/// @brief Implements runtime package manifest JSON loading and validation.

#include <SagaEngine/Packages/PackageManifestLoader.hpp>

#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <string>
#include <unordered_set>
#include <utility>

namespace SagaEngine::Packages
{

namespace
{

constexpr std::uint32_t kSupportedSchemaVersion = 1;

/// Build a structured loader diagnostic with optional reference and field context.
[[nodiscard]] PackageManifestError MakeError(
    std::string diagnosticId,
    std::string message,
    const std::filesystem::path& manifestPath,
    std::optional<std::size_t> referenceIndex = std::nullopt,
    std::optional<std::string> fieldName = std::nullopt)
{
    PackageManifestError error;
    error.diagnosticId = std::move(diagnosticId);
    error.message = std::move(message);
    error.manifestPath = manifestPath;
    error.referenceIndex = referenceIndex;
    error.fieldName = std::move(fieldName);
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

/// Resolve package manifest references against explicit options or package folder.
[[nodiscard]] std::filesystem::path ResolveReferencePath(
    const std::filesystem::path& manifestPath,
    const PackageManifestLoadOptions& options,
    const std::filesystem::path& referencePath)
{
    const std::filesystem::path basePath =
        options.packageBaseDirectory.empty()
            ? manifestPath.parent_path()
            : options.packageBaseDirectory;

    if (basePath.empty())
    {
        return referencePath.lexically_normal();
    }

    return (basePath / referencePath).lexically_normal();
}

/// Validate a required root string field.
[[nodiscard]] bool ValidateRequiredRootStringField(
    const nlohmann::json& root,
    std::string_view field,
    const std::filesystem::path& manifestPath,
    PackageManifestLoadResult& result)
{
    const auto iterator = root.find(field);
    if (iterator == root.end())
    {
        result.errors.push_back(MakeError(
            PackageManifestDiagnostics::MissingField,
            "Package manifest is missing a required field.",
            manifestPath,
            std::nullopt,
            std::string(field)));
        return false;
    }

    if (!iterator->is_string())
    {
        result.errors.push_back(MakeError(
            PackageManifestDiagnostics::InvalidField,
            "Package manifest field must be a string.",
            manifestPath,
            std::nullopt,
            std::string(field)));
        return false;
    }

    if (iterator->get<std::string>().empty())
    {
        result.errors.push_back(MakeError(
            PackageManifestDiagnostics::InvalidField,
            "Package manifest field must not be empty.",
            manifestPath,
            std::nullopt,
            std::string(field)));
        return false;
    }

    return true;
}

/// Parse manifest reference arrays used by package manifests.
void ParseManifestRefs(
    const nlohmann::json& root,
    std::string_view field,
    const std::filesystem::path& manifestPath,
    const PackageManifestLoadOptions& options,
    PackageManifestLoadResult& result,
    std::vector<PackageManifestRef>& output)
{
    const auto iterator = root.find(field);
    if (iterator == root.end())
    {
        result.errors.push_back(MakeError(
            PackageManifestDiagnostics::MissingField,
            "Package manifest is missing a manifest reference array.",
            manifestPath,
            std::nullopt,
            std::string(field)));
        return;
    }

    if (!iterator->is_array())
    {
        result.errors.push_back(MakeError(
            PackageManifestDiagnostics::InvalidField,
            "Package manifest reference field must be an array.",
            manifestPath,
            std::nullopt,
            std::string(field)));
        return;
    }

    std::unordered_set<std::string> ids;
    for (std::size_t index = 0; index < iterator->size(); ++index)
    {
        const auto& item = (*iterator)[index];
        if (!item.is_object())
        {
            result.errors.push_back(MakeError(
                PackageManifestDiagnostics::InvalidField,
                "Manifest reference entry must be a JSON object.",
                manifestPath,
                index,
                std::string(field)));
            continue;
        }

        bool valid = true;
        for (std::string_view requiredField : {"id", "path"})
        {
            const auto refIterator = item.find(requiredField);
            if (refIterator == item.end())
            {
                result.errors.push_back(MakeError(
                    PackageManifestDiagnostics::MissingField,
                    "Manifest reference entry is missing a required field.",
                    manifestPath,
                    index,
                    std::string(requiredField)));
                valid = false;
                continue;
            }

            if (!refIterator->is_string())
            {
                result.errors.push_back(MakeError(
                    PackageManifestDiagnostics::InvalidField,
                    "Manifest reference entry field must be a string.",
                    manifestPath,
                    index,
                    std::string(requiredField)));
                valid = false;
            }
        }

        if (item.contains("hash") && !item["hash"].is_string())
        {
            result.errors.push_back(MakeError(
                PackageManifestDiagnostics::InvalidField,
                "Manifest reference hash must be a string when present.",
                manifestPath,
                index,
                "hash"));
            valid = false;
        }

        if (!valid)
        {
            continue;
        }

        const std::string id = item["id"].get<std::string>();
        if (id.empty() || !ids.insert(id).second)
        {
            result.errors.push_back(MakeError(
                id.empty() ? PackageManifestDiagnostics::InvalidField
                           : PackageManifestDiagnostics::DuplicateId,
                id.empty() ? "Manifest reference id must not be empty."
                           : "Manifest reference array contains a duplicate id.",
                manifestPath,
                index,
                "id"));
            continue;
        }

        const std::string path = item["path"].get<std::string>();
        if (!IsSafeRelativePath(path))
        {
            result.errors.push_back(MakeError(
                PackageManifestDiagnostics::InvalidPath,
                "Manifest reference path must be a non-escaping relative path.",
                manifestPath,
                index,
                "path"));
            continue;
        }

        if (options.validateReferencedManifestFiles)
        {
            const auto resolvedPath =
                ResolveReferencePath(manifestPath, options, path);
            if (!std::filesystem::exists(resolvedPath))
            {
                result.errors.push_back(MakeError(
                    PackageManifestDiagnostics::FileMissing,
                    "Manifest reference points to a file that does not exist.",
                    manifestPath,
                    index,
                    "path"));
                continue;
            }
        }

        PackageManifestRef ref;
        ref.id = id;
        ref.path = path;
        if (item.contains("hash"))
        {
            ref.hash = item["hash"].get<std::string>();
        }
        output.push_back(std::move(ref));
    }
}

/// Parse the optional package asset identity manifest reference.
void ParseAssetIdentityManifestRef(
    const nlohmann::json& root,
    const std::filesystem::path& manifestPath,
    const PackageManifestLoadOptions& options,
    PackageManifestLoadResult& result)
{
    const auto iterator = root.find("assetIdentityManifest");
    if (iterator == root.end())
    {
        return;
    }

    if (!iterator->is_string())
    {
        result.errors.push_back(MakeError(
            PackageManifestDiagnostics::InvalidField,
            "Package manifest assetIdentityManifest must be a string when present.",
            manifestPath,
            std::nullopt,
            "assetIdentityManifest"));
        return;
    }

    const std::string path = iterator->get<std::string>();
    if (!IsSafeRelativePath(path))
    {
        result.errors.push_back(MakeError(
            PackageManifestDiagnostics::InvalidPath,
            "Package manifest assetIdentityManifest must be a non-escaping relative path.",
            manifestPath,
            std::nullopt,
            "assetIdentityManifest"));
        return;
    }

    if (options.validateReferencedManifestFiles)
    {
        const auto resolvedPath =
            ResolveReferencePath(manifestPath, options, path);
        if (!std::filesystem::exists(resolvedPath))
        {
            result.errors.push_back(MakeError(
                PackageManifestDiagnostics::FileMissing,
                "Package manifest assetIdentityManifest points to a file that does not exist.",
                manifestPath,
                std::nullopt,
                "assetIdentityManifest"));
            return;
        }
    }

    result.manifest.assetIdentityManifest = path;
}

} // namespace

std::optional<PackageKind> TryParsePackageKind(std::string_view value) noexcept
{
    if (value == "client")
    {
        return PackageKind::Client;
    }
    if (value == "server")
    {
        return PackageKind::Server;
    }

    return std::nullopt;
}

std::string_view ToString(PackageKind kind) noexcept
{
    switch (kind)
    {
    case PackageKind::Client:
        return "client";
    case PackageKind::Server:
        return "server";
    }

    return "";
}

PackageManifestLoadResult PackageManifestLoader::LoadFromFile(
    const std::filesystem::path& manifestPath)
{
    return LoadFromFile(manifestPath, PackageManifestLoadOptions{});
}

PackageManifestLoadResult PackageManifestLoader::LoadFromFile(
    const std::filesystem::path& manifestPath,
    const PackageManifestLoadOptions& options)
{
    PackageManifestLoadResult result;

    std::ifstream input(manifestPath);
    if (!input.is_open())
    {
        result.errors.push_back(MakeError(
            PackageManifestDiagnostics::ManifestMissing,
            "Package manifest file could not be opened.",
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
            PackageManifestDiagnostics::ParseFailed,
            std::string("Package manifest JSON parse failed: ") + exception.what(),
            manifestPath));
        return result;
    }

    if (!root.is_object())
    {
        result.errors.push_back(MakeError(
            PackageManifestDiagnostics::ParseFailed,
            "Package manifest root must be a JSON object.",
            manifestPath));
        return result;
    }

    const auto schemaVersionIterator = root.find("schemaVersion");
    if (schemaVersionIterator == root.end())
    {
        result.errors.push_back(MakeError(
            PackageManifestDiagnostics::MissingField,
            "Package manifest must contain schemaVersion.",
            manifestPath,
            std::nullopt,
            "schemaVersion"));
        return result;
    }

    if (!schemaVersionIterator->is_number_integer() &&
        !schemaVersionIterator->is_number_unsigned())
    {
        result.errors.push_back(MakeError(
            PackageManifestDiagnostics::InvalidField,
            "Package manifest schemaVersion must be an integer.",
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
            PackageManifestDiagnostics::UnsupportedVersion,
            "Package manifest schemaVersion is not supported.",
            manifestPath,
            std::nullopt,
            "schemaVersion"));
        return result;
    }
    result.manifest.schemaVersion = kSupportedSchemaVersion;

    bool validRoot = true;
    for (std::string_view field :
         {"packageId", "packageKind", "buildProfile", "targetPlatform",
          "runtimeCompatibilityVersion"})
    {
        validRoot = ValidateRequiredRootStringField(
            root, field, manifestPath, result) && validRoot;
    }

    if (root.contains("packageHash") && !root["packageHash"].is_string())
    {
        result.errors.push_back(MakeError(
            PackageManifestDiagnostics::InvalidField,
            "Package manifest packageHash must be a string when present.",
            manifestPath,
            std::nullopt,
            "packageHash"));
        validRoot = false;
    }

    if (!validRoot)
    {
        return result;
    }

    const std::string kindText = root["packageKind"].get<std::string>();
    const auto kind = TryParsePackageKind(kindText);
    if (!kind.has_value())
    {
        result.errors.push_back(MakeError(
            PackageManifestDiagnostics::UnknownKind,
            "Package manifest uses an unknown packageKind.",
            manifestPath,
            std::nullopt,
            "packageKind"));
        return result;
    }

    result.manifest.packageId = root["packageId"].get<std::string>();
    result.manifest.packageKind = *kind;
    result.manifest.buildProfile = root["buildProfile"].get<std::string>();
    result.manifest.targetPlatform = root["targetPlatform"].get<std::string>();
    result.manifest.runtimeCompatibilityVersion =
        root["runtimeCompatibilityVersion"].get<std::string>();
    if (root.contains("packageHash"))
    {
        result.manifest.packageHash = root["packageHash"].get<std::string>();
    }
    ParseAssetIdentityManifestRef(root, manifestPath, options, result);

    ParseManifestRefs(
        root,
        "assetManifests",
        manifestPath,
        options,
        result,
        result.manifest.assetManifests);
    ParseManifestRefs(
        root,
        "artifactManifests",
        manifestPath,
        options,
        result,
        result.manifest.artifactManifests);

    return result;
}

} // namespace SagaEngine::Packages
