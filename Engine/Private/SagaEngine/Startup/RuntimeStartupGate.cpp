/// @file RuntimeStartupGate.cpp
/// @brief Implements thin startup validation orchestration.

#include <SagaEngine/Startup/RuntimeStartupGate.hpp>

#include <SagaEngine/Artifacts/ArtifactStartupValidator.hpp>
#include <SagaEngine/Assets/AssetManifestLoader.hpp>
#include <SagaEngine/Packages/PackageManifestLoader.hpp>
#include <SagaEngine/Packages/PackageStartupValidator.hpp>
#include <SagaEngine/Resources/RuntimeAssetRegistryBootstrapper.h>

#include <filesystem>
#include <utility>

namespace SagaEngine::Startup
{

namespace
{

/// Convert startup domain policy into package manifest kind policy.
[[nodiscard]] Packages::PackageKind ToPackageKind(RuntimeStartupDomain domain) noexcept
{
    switch (domain)
    {
        case RuntimeStartupDomain::Client:
            return Packages::PackageKind::Client;
        case RuntimeStartupDomain::Server:
            return Packages::PackageKind::Server;
    }

    return Packages::PackageKind::Client;
}

/// Resolve package manifest references relative to the package manifest folder.
[[nodiscard]] std::filesystem::path ResolvePackageReference(
    const std::filesystem::path& packageManifestPath,
    const std::filesystem::path& referencePath)
{
    const auto parent = packageManifestPath.parent_path();
    if (parent.empty())
    {
        return referencePath.lexically_normal();
    }

    return (parent / referencePath).lexically_normal();
}

/// Normalize a package diagnostic into the gate result.
void AppendPackageDiagnostic(
    RuntimeStartupGateResult& result,
    const Packages::PackageManifestError& error)
{
    RuntimeStartupGateDiagnostic diagnostic;
    diagnostic.diagnosticId = error.diagnosticId;
    diagnostic.message = error.message;
    diagnostic.manifestPath = error.manifestPath;
    diagnostic.fieldName = error.fieldName;
    diagnostic.referenceIndex = error.referenceIndex;
    result.diagnostics.push_back(std::move(diagnostic));
}

/// Normalize an asset manifest diagnostic into the gate result.
void AppendAssetDiagnostic(
    RuntimeStartupGateResult& result,
    const Assets::AssetManifestError& error)
{
    RuntimeStartupGateDiagnostic diagnostic;
    diagnostic.diagnosticId = error.diagnosticId;
    diagnostic.message = error.message;
    diagnostic.manifestPath = error.manifestPath;
    diagnostic.fieldName = error.fieldName;
    diagnostic.itemIndex = error.assetIndex;
    diagnostic.resolvedPath = error.resolvedPath;
    result.diagnostics.push_back(std::move(diagnostic));
}

/// Normalize an artifact startup diagnostic into the gate result.
void AppendArtifactDiagnostic(
    RuntimeStartupGateResult& result,
    const Artifacts::ArtifactStartupValidationError& error)
{
    RuntimeStartupGateDiagnostic diagnostic;
    diagnostic.diagnosticId = error.diagnosticId;
    diagnostic.message = error.message;
    diagnostic.manifestPath = error.manifestPath;
    diagnostic.fieldName = error.fieldName;
    diagnostic.itemIndex = error.artifactIndex;
    diagnostic.resourceId = error.artifactId;
    diagnostic.resolvedPath = error.resolvedPath;
    result.diagnostics.push_back(std::move(diagnostic));
}

/// Normalize an asset registry bootstrap diagnostic into the gate result.
void AppendAssetBootstrapDiagnostic(
    RuntimeStartupGateResult& result,
    const Resources::RuntimeAssetRegistryBootstrapDiagnostic& error)
{
    RuntimeStartupGateDiagnostic diagnostic;
    diagnostic.diagnosticId = error.diagnosticId;
    diagnostic.message = error.message;
    diagnostic.manifestPath = error.manifestPath;
    diagnostic.referenceIndex = error.referenceIndex;
    diagnostic.itemIndex = error.assetIndex;
    if (!error.assetKey.empty())
    {
        diagnostic.resourceId = error.assetKey;
    }
    diagnostic.resolvedPath = error.resolvedPath;
    result.diagnostics.push_back(std::move(diagnostic));
}

/// Add a missing referenced manifest diagnostic with resolved package path.
void AppendMissingReferenceDiagnostic(
    RuntimeStartupGateResult& result,
    const std::filesystem::path& packageManifestPath,
    const Packages::PackageManifestRef& reference,
    std::size_t referenceIndex,
    const std::filesystem::path& resolvedPath)
{
    RuntimeStartupGateDiagnostic diagnostic;
    diagnostic.diagnosticId = Packages::PackageManifestDiagnostics::FileMissing;
    diagnostic.message = "Package manifest reference points to a file that does not exist.";
    diagnostic.manifestPath = packageManifestPath;
    diagnostic.fieldName = "path";
    diagnostic.referenceIndex = referenceIndex;
    diagnostic.resourceId = reference.id;
    diagnostic.resolvedPath = resolvedPath;
    result.diagnostics.push_back(std::move(diagnostic));
}

/// Validate referenced asset manifests without owning asset loading behavior.
void ValidateAssetManifestReferences(
    RuntimeStartupGateResult& result,
    const std::filesystem::path& packageManifestPath,
    const RuntimeStartupGateOptions& options)
{
    for (std::size_t index = 0; index < result.packageManifest.assetManifests.size(); ++index)
    {
        const auto& reference = result.packageManifest.assetManifests[index];
        const auto resolvedPath = ResolvePackageReference(packageManifestPath, reference.path);
        if (!std::filesystem::exists(resolvedPath))
        {
            AppendMissingReferenceDiagnostic(
                result,
                packageManifestPath,
                reference,
                index,
                resolvedPath);
            continue;
        }

        Assets::AssetManifestLoadOptions loadOptions;
        loadOptions.validateAssetFiles = options.validateAssetFiles;
        loadOptions.assetBaseDirectory = resolvedPath.parent_path();

        const auto assetResult =
            Assets::AssetManifestLoader::LoadFromFile(resolvedPath, loadOptions);
        for (const auto& error : assetResult.errors)
        {
            AppendAssetDiagnostic(result, error);
        }
    }
}

/// Validate identity-backed package asset coverage without mutating runtime state.
void ValidateIdentityBackedAssetManifestReferences(
    RuntimeStartupGateResult& result,
    const RuntimeStartupGateOptions& options)
{
    Resources::RuntimeAssetRegistryBootstrapOptions bootstrapOptions;
    bootstrapOptions.packageManifestPath = options.packageManifestPath;
    bootstrapOptions.validateAssetFiles = options.validateAssetFiles;

    const Resources::RuntimeAssetRegistryBootstrapResult bootstrapResult =
        Resources::RuntimeAssetRegistryBootstrapper::
            ValidatePackageAssetsFromPackageIdentityManifest(
                result.packageManifest,
                bootstrapOptions);
    for (const Resources::RuntimeAssetRegistryBootstrapDiagnostic& error :
         bootstrapResult.diagnostics)
    {
        AppendAssetBootstrapDiagnostic(result, error);
    }
}

/// Validate referenced artifact manifests without loading artifact payloads.
void ValidateArtifactManifestReferences(
    RuntimeStartupGateResult& result,
    const std::filesystem::path& packageManifestPath)
{
    for (std::size_t index = 0; index < result.packageManifest.artifactManifests.size(); ++index)
    {
        const auto& reference = result.packageManifest.artifactManifests[index];
        const auto resolvedPath = ResolvePackageReference(packageManifestPath, reference.path);
        if (!std::filesystem::exists(resolvedPath))
        {
            AppendMissingReferenceDiagnostic(
                result,
                packageManifestPath,
                reference,
                index,
                resolvedPath);
            continue;
        }

        const auto artifactResult =
            Artifacts::ArtifactStartupValidator::ValidateManifestForStartup(resolvedPath);
        for (const auto& error : artifactResult.errors)
        {
            AppendArtifactDiagnostic(result, error);
        }
    }
}

} // namespace

RuntimeStartupGateResult RuntimeStartupGate::ValidatePackageForStartup(
    const RuntimeStartupGateOptions& options)
{
    RuntimeStartupGateResult result;

    Packages::PackageStartupValidationOptions packageOptions;
    packageOptions.expectedPackageKind = ToPackageKind(options.expectedDomain);
    packageOptions.runtimeCompatibilityVersion =
        options.expectedRuntimeCompatibilityVersion;
    packageOptions.validateReferencedManifestFiles = false;

    const auto packageResult =
        Packages::PackageStartupValidator::ValidateManifestForStartup(
            options.packageManifestPath,
            packageOptions);

    result.packageManifest = packageResult.manifest;
    for (const auto& error : packageResult.errors)
    {
        AppendPackageDiagnostic(result, error);
    }

    if (result.packageManifest.schemaVersion == 0 ||
        !options.validateReferencedManifestFiles)
    {
        return result;
    }

    if (result.packageManifest.assetIdentityManifest.has_value())
    {
        ValidateIdentityBackedAssetManifestReferences(result, options);
    }
    else
    {
        ValidateAssetManifestReferences(result, options.packageManifestPath, options);
    }
    ValidateArtifactManifestReferences(result, options.packageManifestPath);

    return result;
}

} // namespace SagaEngine::Startup
