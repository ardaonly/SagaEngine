/// @file RuntimeAssetRegistryBootstrapper.cpp
/// @brief Package asset registry bootstrap implementation.

#include "SagaEngine/Resources/RuntimeAssetRegistryBootstrapper.h"

#include "SagaEngine/Assets/AssetManifestLoader.hpp"
#include "SagaEngine/Resources/AssetIdentityManifest.h"

#include <optional>
#include <unordered_map>
#include <utility>

namespace SagaEngine::Resources {
namespace {

/// One package-level asset registration planned before registry mutation.
struct PlannedPackageAsset
{
    AssetManifestRegistryAdapterPlannedEntry plannedEntry; ///< Adapter-planned registry entry.
    std::filesystem::path manifestPath;                    ///< Asset manifest path that produced the entry.
    std::size_t referenceIndex = 0;                         ///< Package asset manifest reference index.
};

/// Package asset plan produced before registry mutation.
struct PackageAssetPlanResult
{
    std::vector<PlannedPackageAsset> plannedAssets; ///< Assets ready for registry collision checks or validation success.
    RuntimeAssetRegistryBootstrapResult result;     ///< Diagnostics collected while building the plan.
};

/// Resolve package manifest references relative to the package manifest folder.
[[nodiscard]] std::filesystem::path ResolvePackageReference(
    const std::filesystem::path& packageManifestPath,
    const std::filesystem::path& packageBaseDirectory,
    const std::filesystem::path& referencePath)
{
    const std::filesystem::path basePath =
        packageBaseDirectory.empty()
            ? packageManifestPath.parent_path()
            : packageBaseDirectory;
    if (basePath.empty())
    {
        return referencePath.lexically_normal();
    }

    return (basePath / referencePath).lexically_normal();
}

/// Build a bootstrap diagnostic with optional package and asset context.
[[nodiscard]] RuntimeAssetRegistryBootstrapDiagnostic MakeDiagnostic(
    std::string diagnosticId,
    std::string message,
    const std::filesystem::path& manifestPath,
    std::optional<std::size_t> referenceIndex,
    std::optional<std::size_t> assetIndex,
    AssetKey assetKey,
    AssetId assetId,
    std::optional<std::filesystem::path> resolvedPath = std::nullopt)
{
    RuntimeAssetRegistryBootstrapDiagnostic diagnostic;
    diagnostic.diagnosticId = std::move(diagnosticId);
    diagnostic.message = std::move(message);
    diagnostic.manifestPath = manifestPath;
    diagnostic.referenceIndex = referenceIndex;
    diagnostic.assetIndex = assetIndex;
    diagnostic.assetKey = std::move(assetKey);
    diagnostic.assetId = assetId;
    diagnostic.resolvedPath = std::move(resolvedPath);
    return diagnostic;
}

/// Normalize an asset manifest loader diagnostic into bootstrap output.
void AppendAssetLoaderDiagnostic(
    RuntimeAssetRegistryBootstrapResult& result,
    const Assets::AssetManifestError& error,
    std::size_t referenceIndex)
{
    result.diagnostics.push_back(MakeDiagnostic(
        error.diagnosticId,
        error.message,
        error.manifestPath,
        referenceIndex,
        error.assetIndex,
        {},
        kInvalidAssetId,
        error.resolvedPath));
}

/// Normalize an adapter diagnostic into bootstrap output.
void AppendAdapterDiagnostic(
    RuntimeAssetRegistryBootstrapResult& result,
    const AssetManifestRegistryAdapterDiagnostic& diagnostic,
    std::size_t referenceIndex)
{
    result.diagnostics.push_back(MakeDiagnostic(
        diagnostic.diagnosticId,
        diagnostic.message,
        diagnostic.manifestPath,
        referenceIndex,
        diagnostic.assetIndex,
        diagnostic.assetKey,
        diagnostic.assetId,
        diagnostic.resolvedPath));
}

/// Normalize a registry batch diagnostic into bootstrap output.
void AppendRegistryDiagnostic(
    RuntimeAssetRegistryBootstrapResult& result,
    const AssetRegistryInsertResult& diagnostic,
    const RuntimeAssetRegistryBootstrapOptions& options)
{
    result.diagnostics.push_back(MakeDiagnostic(
        diagnostic.diagnosticId,
        diagnostic.message,
        options.packageManifestPath,
        std::nullopt,
        std::nullopt,
        diagnostic.assetKey,
        diagnostic.assetId));
}

/// Normalize an identity manifest loader diagnostic into bootstrap output.
void AppendIdentityManifestDiagnostic(
    RuntimeAssetRegistryBootstrapResult& result,
    const AssetIdentityManifestError& error)
{
    result.diagnostics.push_back(MakeDiagnostic(
        error.diagnosticId,
        error.message,
        error.manifestPath,
        std::nullopt,
        error.mappingIndex,
        error.assetKey,
        error.assetId));
}

/// Load the package identity manifest and build an explicit resolver.
[[nodiscard]] std::optional<StaticAssetIdResolver> LoadPackageIdentityResolver(
    const Packages::PackageManifest& packageManifest,
    const RuntimeAssetRegistryBootstrapOptions& options,
    RuntimeAssetRegistryBootstrapResult& result)
{
    if (!packageManifest.assetIdentityManifest.has_value())
    {
        result.diagnostics.push_back(MakeDiagnostic(
            RuntimeAssetRegistryBootstrapDiagnostics::MissingAssetIdentityManifest,
            "Package manifest does not reference an asset identity manifest.",
            options.packageManifestPath,
            std::nullopt,
            std::nullopt,
            {},
            kInvalidAssetId));
        return std::nullopt;
    }

    const std::filesystem::path identityManifestPath =
        ResolvePackageReference(
            options.packageManifestPath,
            options.packageBaseDirectory,
            *packageManifest.assetIdentityManifest);
    const AssetIdentityManifestLoadResult identityLoadResult =
        AssetIdentityManifestLoader::LoadFromFile(identityManifestPath);
    for (const AssetIdentityManifestError& error : identityLoadResult.errors)
    {
        AppendIdentityManifestDiagnostic(result, error);
    }

    if (!identityLoadResult.Succeeded())
    {
        return std::nullopt;
    }

    return AssetIdentityManifestLoader::BuildResolver(identityLoadResult.manifest);
}

/// Build a package-level asset plan without mutating a registry.
[[nodiscard]] PackageAssetPlanResult PlanPackageAssets(
    const Packages::PackageManifest& packageManifest,
    const IAssetIdResolver& assetIdResolver,
    const RuntimeAssetRegistryBootstrapOptions& options)
{
    PackageAssetPlanResult planResult;

    for (std::size_t index = 0; index < packageManifest.assetManifests.size(); ++index)
    {
        const Packages::PackageManifestRef& reference =
            packageManifest.assetManifests[index];
        const std::filesystem::path resolvedManifestPath =
            ResolvePackageReference(options.packageManifestPath,
                                    options.packageBaseDirectory,
                                    reference.path);

        Assets::AssetManifestLoadOptions loadOptions;
        loadOptions.validateAssetFiles = options.validateAssetFiles;
        loadOptions.assetBaseDirectory = resolvedManifestPath.parent_path();

        const Assets::AssetManifestLoadResult assetLoadResult =
            Assets::AssetManifestLoader::LoadFromFile(
                resolvedManifestPath,
                loadOptions);
        for (const Assets::AssetManifestError& error : assetLoadResult.errors)
        {
            AppendAssetLoaderDiagnostic(planResult.result, error, index);
        }

        if (!assetLoadResult.Succeeded())
        {
            continue;
        }

        AssetManifestRegistryAdapterOptions adapterOptions;
        adapterOptions.assetManifestPath = resolvedManifestPath;
        adapterOptions.assetBaseDirectory = resolvedManifestPath.parent_path();

        const AssetManifestRegistryAdapterPlanResult plan =
            AssetManifestRegistryAdapter::PlanManifestAssets(
                assetLoadResult.manifest,
                assetIdResolver,
                adapterOptions);
        for (const AssetManifestRegistryAdapterDiagnostic& diagnostic :
             plan.diagnostics)
        {
            AppendAdapterDiagnostic(planResult.result, diagnostic, index);
        }

        if (!plan.Succeeded())
        {
            continue;
        }

        for (const AssetManifestRegistryAdapterPlannedEntry& planned :
             plan.plannedEntries)
        {
            PlannedPackageAsset packageAsset;
            packageAsset.plannedEntry = planned;
            packageAsset.manifestPath = resolvedManifestPath;
            packageAsset.referenceIndex = index;
            planResult.plannedAssets.push_back(std::move(packageAsset));
        }
    }

    if (!planResult.result.diagnostics.empty())
    {
        return planResult;
    }

    std::unordered_map<AssetKey, std::size_t> seenKeys;
    std::unordered_map<AssetId, AssetKey> seenIds;
    for (std::size_t index = 0; index < planResult.plannedAssets.size(); ++index)
    {
        const PlannedPackageAsset& planned = planResult.plannedAssets[index];
        const AssetRegistryEntry& entry = planned.plannedEntry.entry;

        const auto existingKey = seenKeys.find(entry.assetKey);
        if (existingKey != seenKeys.end())
        {
            planResult.result.diagnostics.push_back(MakeDiagnostic(
                RuntimeAssetRegistryBootstrapDiagnostics::DuplicatePackageAssetKey,
                "Package asset manifests contain the same AssetKey more than once.",
                planned.manifestPath,
                planned.referenceIndex,
                planned.plannedEntry.assetIndex,
                entry.assetKey,
                entry.assetId,
                planned.plannedEntry.resolvedPath));
        }
        else
        {
            seenKeys.emplace(entry.assetKey, index);
        }

        const auto existingId = seenIds.find(entry.assetId);
        if (existingId != seenIds.end() && existingId->second != entry.assetKey)
        {
            planResult.result.diagnostics.push_back(MakeDiagnostic(
                RuntimeAssetRegistryBootstrapDiagnostics::DuplicatePackageAssetId,
                "Package asset manifests resolve multiple AssetKeys to the same AssetId.",
                planned.manifestPath,
                planned.referenceIndex,
                planned.plannedEntry.assetIndex,
                entry.assetKey,
                entry.assetId,
                planned.plannedEntry.resolvedPath));
        }
        else
        {
            seenIds.emplace(entry.assetId, entry.assetKey);
        }
    }

    return planResult;
}

} // namespace

RuntimeAssetRegistryBootstrapResult
RuntimeAssetRegistryBootstrapper::ValidatePackageAssetsFromPackageIdentityManifest(
    const Packages::PackageManifest& packageManifest,
    const RuntimeAssetRegistryBootstrapOptions& options)
{
    RuntimeAssetRegistryBootstrapResult result;
    const std::optional<StaticAssetIdResolver> resolver =
        LoadPackageIdentityResolver(packageManifest, options, result);
    if (!resolver.has_value())
    {
        return result;
    }

    return PlanPackageAssets(packageManifest, *resolver, options).result;
}

RuntimeAssetRegistryBootstrapResult
RuntimeAssetRegistryBootstrapper::RegisterPackageAssetsFromPackageIdentityManifest(
    AssetRegistry& registry,
    const Packages::PackageManifest& packageManifest,
    const RuntimeAssetRegistryBootstrapOptions& options)
{
    RuntimeAssetRegistryBootstrapResult result;
    const std::optional<StaticAssetIdResolver> resolver =
        LoadPackageIdentityResolver(packageManifest, options, result);
    if (!resolver.has_value())
    {
        return result;
    }

    return RegisterPackageAssets(registry, packageManifest, *resolver, options);
}

RuntimeAssetRegistryBootstrapResult
RuntimeAssetRegistryBootstrapper::RegisterPackageAssets(
    AssetRegistry& registry,
    const Packages::PackageManifest& packageManifest,
    const IAssetIdResolver& assetIdResolver,
    const RuntimeAssetRegistryBootstrapOptions& options)
{
    PackageAssetPlanResult plan =
        PlanPackageAssets(packageManifest, assetIdResolver, options);
    RuntimeAssetRegistryBootstrapResult result = std::move(plan.result);
    if (!result.diagnostics.empty())
    {
        return result;
    }

    for (const PlannedPackageAsset& planned : plan.plannedAssets)
    {
        const AssetRegistryEntry& entry = planned.plannedEntry.entry;

        if (registry.FindByKey(entry.assetKey) != nullptr)
        {
            result.diagnostics.push_back(MakeDiagnostic(
                RuntimeAssetRegistryBootstrapDiagnostics::RegistryAssetKeyCollision,
                "AssetRegistry already contains this AssetKey.",
                planned.manifestPath,
                planned.referenceIndex,
                planned.plannedEntry.assetIndex,
                entry.assetKey,
                entry.assetId,
                planned.plannedEntry.resolvedPath));
        }

        if (registry.Find(entry.assetId) != nullptr)
        {
            result.diagnostics.push_back(MakeDiagnostic(
                RuntimeAssetRegistryBootstrapDiagnostics::RegistryAssetIdCollision,
                "AssetRegistry already contains this AssetId.",
                planned.manifestPath,
                planned.referenceIndex,
                planned.plannedEntry.assetIndex,
                entry.assetKey,
                entry.assetId,
                planned.plannedEntry.resolvedPath));
        }
    }

    if (!result.diagnostics.empty())
    {
        return result;
    }

    std::vector<AssetRegistryEntry> entries;
    entries.reserve(plan.plannedAssets.size());
    for (const PlannedPackageAsset& planned : plan.plannedAssets)
    {
        entries.push_back(planned.plannedEntry.entry);
    }

    const AssetRegistryBatchInsertResult insertResult =
        registry.TryInsertAll(std::move(entries));
    result.registeredAssetCount = insertResult.insertedCount;
    for (const AssetRegistryInsertResult& diagnostic : insertResult.diagnostics)
    {
        AppendRegistryDiagnostic(result, diagnostic, options);
    }

    return result;
}

} // namespace SagaEngine::Resources
