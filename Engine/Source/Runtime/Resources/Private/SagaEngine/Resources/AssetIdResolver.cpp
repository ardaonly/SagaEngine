/// @file AssetIdResolver.cpp
/// @brief Explicit AssetKey to AssetId resolution implementation.

#include "SagaEngine/Resources/AssetIdResolver.h"

#include <utility>

namespace SagaEngine::Resources {
namespace {

[[nodiscard]] AssetIdResolutionDiagnostic MakeDiagnostic(
    const char* diagnosticId,
    std::string message,
    AssetKey assetKey,
    AssetId assetId,
    std::size_t assetIndex)
{
    AssetIdResolutionDiagnostic diagnostic;
    diagnostic.diagnosticId = diagnosticId;
    diagnostic.message = std::move(message);
    diagnostic.assetKey = std::move(assetKey);
    diagnostic.assetId = assetId;
    diagnostic.assetIndex = assetIndex;
    return diagnostic;
}

} // namespace

void StaticAssetIdResolver::AddMapping(AssetKey assetKey, AssetId assetId)
{
    mappings_[std::move(assetKey)] = assetId;
}

bool StaticAssetIdResolver::ResolveAssetId(
    std::string_view assetKey,
    AssetId& outAssetId) const
{
    const auto iterator = mappings_.find(AssetKey(assetKey));
    if (iterator == mappings_.end())
    {
        outAssetId = kInvalidAssetId;
        return false;
    }

    outAssetId = iterator->second;
    return true;
}

AssetIdResolutionResult AssetIdResolver::ResolveAssetKeys(
    const std::vector<AssetKey>& assetKeys,
    const IAssetIdResolver& resolver)
{
    AssetIdResolutionResult result;
    std::unordered_map<AssetKey, std::size_t> seenKeys;
    std::unordered_map<AssetId, AssetKey> seenIds;

    for (std::size_t index = 0; index < assetKeys.size(); ++index)
    {
        const AssetKey& assetKey = assetKeys[index];
        if (assetKey.empty())
        {
            result.diagnostics.push_back(MakeDiagnostic(
                AssetIdResolverDiagnostics::InvalidAssetKey,
                "Asset key must not be empty.",
                assetKey,
                kInvalidAssetId,
                index));
            continue;
        }

        if (seenKeys.find(assetKey) != seenKeys.end())
        {
            result.diagnostics.push_back(MakeDiagnostic(
                AssetIdResolverDiagnostics::DuplicateAssetKey,
                "Asset key appears more than once in the resolution request.",
                assetKey,
                kInvalidAssetId,
                index));
            continue;
        }
        seenKeys.emplace(assetKey, index);

        AssetId assetId = kInvalidAssetId;
        if (!resolver.ResolveAssetId(assetKey, assetId))
        {
            result.diagnostics.push_back(MakeDiagnostic(
                AssetIdResolverDiagnostics::MissingKey,
                "Asset key has no explicit AssetId mapping.",
                assetKey,
                kInvalidAssetId,
                index));
            continue;
        }

        if (assetId == kInvalidAssetId)
        {
            result.diagnostics.push_back(MakeDiagnostic(
                AssetIdResolverDiagnostics::InvalidAssetId,
                "Asset key resolved to the invalid AssetId.",
                assetKey,
                assetId,
                index));
            continue;
        }

        const auto existingId = seenIds.find(assetId);
        if (existingId != seenIds.end() && existingId->second != assetKey)
        {
            result.diagnostics.push_back(MakeDiagnostic(
                AssetIdResolverDiagnostics::DuplicateAssetId,
                "Multiple asset keys resolved to the same AssetId.",
                assetKey,
                assetId,
                index));
            continue;
        }
        seenIds.emplace(assetId, assetKey);

        AssetIdResolution resolution;
        resolution.assetKey = assetKey;
        resolution.assetId = assetId;
        result.resolutions.push_back(std::move(resolution));
    }

    return result;
}

} // namespace SagaEngine::Resources
