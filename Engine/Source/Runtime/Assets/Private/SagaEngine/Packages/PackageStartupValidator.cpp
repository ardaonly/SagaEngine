/// @file PackageStartupValidator.cpp
/// @brief Implements runtime package startup acceptance policy.

#include <SagaEngine/Packages/PackageStartupValidator.hpp>

#include <utility>

namespace SagaEngine::Packages
{

namespace
{

/// Build a startup policy diagnostic attached to the package manifest.
[[nodiscard]] PackageManifestError MakePolicyError(
    std::string diagnosticId,
    std::string message,
    const std::filesystem::path& manifestPath,
    std::optional<std::string> fieldName)
{
    PackageManifestError error;
    error.diagnosticId = std::move(diagnosticId);
    error.message = std::move(message);
    error.manifestPath = manifestPath;
    error.fieldName = std::move(fieldName);
    return error;
}

} // namespace

PackageStartupValidationResult PackageStartupValidator::ValidateManifestForStartup(
    const std::filesystem::path& manifestPath)
{
    return ValidateManifestForStartup(manifestPath, PackageStartupValidationOptions{});
}

PackageStartupValidationResult PackageStartupValidator::ValidateManifestForStartup(
    const std::filesystem::path& manifestPath,
    const PackageStartupValidationOptions& options)
{
    PackageManifestLoadOptions loadOptions;
    loadOptions.validateReferencedManifestFiles =
        options.validateReferencedManifestFiles;
    loadOptions.packageBaseDirectory = options.packageBaseDirectory;

    const auto loadResult =
        PackageManifestLoader::LoadFromFile(manifestPath, loadOptions);

    PackageStartupValidationResult result;
    result.manifest = loadResult.manifest;
    result.errors = loadResult.errors;
    if (!loadResult.Succeeded())
    {
        return result;
    }

    if (options.expectedPackageKind.has_value() &&
        result.manifest.packageKind != *options.expectedPackageKind)
    {
        result.errors.push_back(MakePolicyError(
            PackageStartupDiagnostics::WrongPackageKind,
            "Package manifest kind does not match the expected startup domain.",
            manifestPath,
            "packageKind"));
    }

    if (options.runtimeCompatibilityVersion.has_value() &&
        result.manifest.runtimeCompatibilityVersion !=
            *options.runtimeCompatibilityVersion)
    {
        result.errors.push_back(MakePolicyError(
            PackageStartupDiagnostics::IncompatibleRuntime,
            "Package manifest runtimeCompatibilityVersion is not accepted.",
            manifestPath,
            "runtimeCompatibilityVersion"));
    }

    return result;
}

} // namespace SagaEngine::Packages
