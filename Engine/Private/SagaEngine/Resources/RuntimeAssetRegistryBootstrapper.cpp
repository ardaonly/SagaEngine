/// @file RuntimeAssetRegistryBootstrapper.cpp
/// @brief Package asset registry bootstrap implementation.

#include "SagaEngine/Resources/RuntimeAssetRegistryBootstrapper.h"

#include "SagaEngine/Assets/AssetManifestLoader.hpp"

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

/// Resolve package manifest references relative to the package manifest folder.
[[nodiscard]] std::filesystem::path ResolvePackageReference(
    const std::filesystem::path& packageManifestPath,
    const std::filesystem::path& referencePath)
{
    const std::filesystem::path parent = packageManifestPath.parent_path();
    if (parent.empty())
    {
        return referencePath.lexically_normal();
    }

    return (parent / referencePath).lexically_normal();
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

} // namespace

RuntimeAssetRegistryBootstrapResult
RuntimeAssetRegistryBootstrapper::RegisterPackageAssets(
    AssetRegistry& registry,
    const Packages::PackageManifest& packageManifest,
    const IAssetIdResolver& assetIdResolver,
    const RuntimeAssetRegistryBootstrapOptions& options)
{
    RuntimeAssetRegistryBootstrapResult result;
    std::vector<PlannedPackageAsset> plannedAssets;

    for (std::size_t index = 0; index < packageManifest.assetManifests.size(); ++index)
    {
        const Packages::PackageManifestRef& reference =
            packageManifest.assetManifests[index];
        const std::filesystem::path resolvedManifestPath =
            ResolvePackageReference(options.packageManifestPath, reference.path);

        Assets::AssetManifestLoadOptions loadOptions;
        loadOptions.validateAssetFiles = options.validateAssetFiles;
        loadOptions.assetBaseDirectory = resolvedManifestPath.parent_path();

        const Assets::AssetManifestLoadResult assetLoadResult =
            Assets::AssetManifestLoader::LoadFromFile(
                resolvedManifestPath,
                loadOptions);
        for (const Assets::AssetManifestError& error : assetLoadResult.errors)
        {
            AppendAssetLoaderDiagnostic(result, error, index);
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
            AppendAdapterDiagnostic(result, diagnostic, index);
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
            plannedAssets.push_back(std::move(packageAsset));
        }
    }

    if (!result.diagnostics.empty())
    {
        return result;
    }

    std::unordered_map<AssetKey, std::size_t> seenKeys;
    std::unordered_map<AssetId, AssetKey> seenIds;
    for (std::size_t index = 0; index < plannedAssets.size(); ++index)
    {
        const PlannedPackageAsset& planned = plannedAssets[index];
        const AssetRegistryEntry& entry = planned.plannedEntry.entry;

        const auto existingKey = seenKeys.find(entry.assetKey);
        if (existingKey != seenKeys.end())
        {
            result.diagnostics.push_back(MakeDiagnostic(
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
            result.diagnostics.push_back(MakeDiagnostic(
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
    entries.reserve(plannedAssets.size());
    for (const PlannedPackageAsset& planned : plannedAssets)
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
