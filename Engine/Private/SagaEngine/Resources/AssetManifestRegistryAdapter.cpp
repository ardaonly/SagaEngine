/// @file AssetManifestRegistryAdapter.cpp
/// @brief Asset manifest to registry adapter implementation.

#include "SagaEngine/Resources/AssetManifestRegistryAdapter.h"

#include <unordered_map>
#include <utility>

namespace SagaEngine::Resources {
namespace {

[[nodiscard]] AssetManifestRegistryAdapterDiagnostic MakeDiagnostic(
    const char* diagnosticId,
    std::string message,
    AssetKey assetKey,
    AssetId assetId,
    std::optional<std::size_t> assetIndex,
    const std::filesystem::path& manifestPath,
    std::optional<std::filesystem::path> resolvedPath = std::nullopt)
{
    AssetManifestRegistryAdapterDiagnostic diagnostic;
    diagnostic.diagnosticId = diagnosticId;
    diagnostic.message = std::move(message);
    diagnostic.assetKey = std::move(assetKey);
    diagnostic.assetId = assetId;
    diagnostic.assetIndex = assetIndex;
    diagnostic.manifestPath = manifestPath;
    diagnostic.resolvedPath = std::move(resolvedPath);
    return diagnostic;
}

/// Resolve runtime/cooked asset file references like AssetManifestLoader.
[[nodiscard]] std::filesystem::path ResolveRuntimeAssetPath(
    const AssetManifestRegistryAdapterOptions& options,
    const std::filesystem::path& assetPath)
{
    const std::filesystem::path basePath =
        options.assetBaseDirectory.empty()
            ? options.assetManifestPath.parent_path()
            : options.assetBaseDirectory;

    if (basePath.empty())
    {
        return assetPath.lexically_normal();
    }

    return (basePath / assetPath).lexically_normal();
}

/// Convert manifest asset kind into the registry's runtime resource kind.
[[nodiscard]] std::optional<AssetKind> MapManifestKind(
    Assets::AssetKind manifestKind) noexcept
{
    switch (manifestKind)
    {
        case Assets::AssetKind::Mesh:
            return AssetKind::Mesh;
        case Assets::AssetKind::Texture:
            return AssetKind::Texture;
        case Assets::AssetKind::Shader:
            return AssetKind::Shader;
        case Assets::AssetKind::Audio:
            return AssetKind::Audio;
        case Assets::AssetKind::Animation:
            return AssetKind::Animation;
        case Assets::AssetKind::Material:
            return AssetKind::MaterialAsset;
    }

    return std::nullopt;
}

} // namespace

AssetManifestRegistryAdapterPlanResult
AssetManifestRegistryAdapter::PlanManifestAssets(
    const Assets::AssetManifest& manifest,
    const IAssetIdResolver& assetIdResolver,
    const AssetManifestRegistryAdapterOptions& options)
{
    AssetManifestRegistryAdapterPlanResult result;
    std::unordered_map<AssetKey, std::size_t> seenKeys;
    std::unordered_map<AssetId, AssetKey> seenIds;
    result.plannedEntries.reserve(manifest.assets.size());

    for (std::size_t index = 0; index < manifest.assets.size(); ++index)
    {
        const Assets::AssetManifestEntry& asset = manifest.assets[index];
        const AssetKey assetKey = asset.id;
        const std::filesystem::path resolvedPath =
            ResolveRuntimeAssetPath(options, asset.path);

        bool entryValid = true;
        if (assetKey.empty())
        {
            result.diagnostics.push_back(MakeDiagnostic(
                AssetManifestRegistryAdapterDiagnostics::InvalidAssetKey,
                "Asset manifest entry has an empty AssetKey.",
                assetKey,
                kInvalidAssetId,
                index,
                options.assetManifestPath,
                resolvedPath));
            entryValid = false;
        }
        else if (seenKeys.find(assetKey) != seenKeys.end())
        {
            result.diagnostics.push_back(MakeDiagnostic(
                AssetManifestRegistryAdapterDiagnostics::DuplicateManifestAssetKey,
                "Asset manifest contains the same AssetKey more than once.",
                assetKey,
                kInvalidAssetId,
                index,
                options.assetManifestPath,
                resolvedPath));
            entryValid = false;
        }
        else
        {
            seenKeys.emplace(assetKey, index);
        }

        const std::optional<AssetKind> registryKind =
            MapManifestKind(asset.kind);
        if (!registryKind.has_value())
        {
            result.diagnostics.push_back(MakeDiagnostic(
                AssetManifestRegistryAdapterDiagnostics::UnsupportedKind,
                "Asset manifest entry uses a kind that cannot be registered.",
                assetKey,
                kInvalidAssetId,
                index,
                options.assetManifestPath,
                resolvedPath));
            entryValid = false;
        }

        AssetId assetId = kInvalidAssetId;
        if (!assetKey.empty())
        {
            if (!assetIdResolver.ResolveAssetId(assetKey, assetId))
            {
                result.diagnostics.push_back(MakeDiagnostic(
                    AssetManifestRegistryAdapterDiagnostics::MissingAssetIdMapping,
                    "AssetKey has no explicit AssetId mapping.",
                    assetKey,
                    kInvalidAssetId,
                    index,
                    options.assetManifestPath,
                    resolvedPath));
                entryValid = false;
            }
            else if (assetId == kInvalidAssetId)
            {
                result.diagnostics.push_back(MakeDiagnostic(
                    AssetManifestRegistryAdapterDiagnostics::InvalidResolvedAssetId,
                    "AssetKey resolved to the invalid AssetId.",
                    assetKey,
                    assetId,
                    index,
                    options.assetManifestPath,
                    resolvedPath));
                entryValid = false;
            }
            else
            {
                const auto existingResolvedId = seenIds.find(assetId);
                if (existingResolvedId != seenIds.end() &&
                    existingResolvedId->second != assetKey)
                {
                    result.diagnostics.push_back(MakeDiagnostic(
                        AssetManifestRegistryAdapterDiagnostics::DuplicateResolvedAssetId,
                        "Multiple AssetKeys resolved to the same AssetId.",
                        assetKey,
                        assetId,
                        index,
                        options.assetManifestPath,
                        resolvedPath));
                    entryValid = false;
                }
                else
                {
                    seenIds.emplace(assetId, assetKey);
                }
            }
        }

        if (!entryValid)
        {
            continue;
        }

        AssetRegistryEntry entry;
        entry.assetId = assetId;
        entry.assetKey = assetKey;
        entry.kind = *registryKind;
        entry.sourcePath = resolvedPath.string();
        AssetManifestRegistryAdapterPlannedEntry plannedEntry;
        plannedEntry.entry = std::move(entry);
        plannedEntry.assetIndex = index;
        plannedEntry.resolvedPath = resolvedPath;
        result.plannedEntries.push_back(std::move(plannedEntry));
    }

    return result;
}

AssetManifestRegistryAdapterResult
AssetManifestRegistryAdapter::RegisterManifestAssets(
    AssetRegistry& registry,
    const Assets::AssetManifest& manifest,
    const IAssetIdResolver& assetIdResolver,
    const AssetManifestRegistryAdapterOptions& options)
{
    AssetManifestRegistryAdapterResult result;
    const AssetManifestRegistryAdapterPlanResult plan =
        PlanManifestAssets(manifest, assetIdResolver, options);
    result.diagnostics = plan.diagnostics;

    if (!result.diagnostics.empty())
    {
        return result;
    }

    std::vector<AssetRegistryEntry> entries;
    entries.reserve(plan.plannedEntries.size());
    for (const AssetManifestRegistryAdapterPlannedEntry& planned : plan.plannedEntries)
    {
        if (registry.FindByKey(planned.entry.assetKey) != nullptr)
        {
            result.diagnostics.push_back(MakeDiagnostic(
                AssetManifestRegistryAdapterDiagnostics::RegistryAssetKeyCollision,
                "AssetRegistry already contains this AssetKey.",
                planned.entry.assetKey,
                planned.entry.assetId,
                planned.assetIndex,
                options.assetManifestPath,
                planned.resolvedPath));
        }

        if (registry.Find(planned.entry.assetId) != nullptr)
        {
            result.diagnostics.push_back(MakeDiagnostic(
                AssetManifestRegistryAdapterDiagnostics::RegistryAssetIdCollision,
                "AssetRegistry already contains this AssetId.",
                planned.entry.assetKey,
                planned.entry.assetId,
                planned.assetIndex,
                options.assetManifestPath,
                planned.resolvedPath));
        }

        entries.push_back(planned.entry);
    }

    if (!result.diagnostics.empty())
    {
        return result;
    }

    const AssetRegistryBatchInsertResult insertResult =
        registry.TryInsertAll(std::move(entries));
    result.registeredAssetCount = insertResult.insertedCount;
    for (const AssetRegistryInsertResult& diagnostic : insertResult.diagnostics)
    {
        result.diagnostics.push_back(MakeDiagnostic(
            diagnostic.diagnosticId.c_str(),
            diagnostic.message,
            diagnostic.assetKey,
            diagnostic.assetId,
            std::nullopt,
            options.assetManifestPath));
    }

    return result;
}

} // namespace SagaEngine::Resources
