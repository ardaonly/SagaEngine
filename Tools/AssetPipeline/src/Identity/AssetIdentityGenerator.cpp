/// @file AssetIdentityGenerator.cpp
/// @brief Implements deterministic AssetKey to AssetId allocation.

#include "SagaAssetPipeline/Identity/AssetIdentityGenerator.hpp"

#include <algorithm>
#include <limits>
#include <map>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace SagaAssetPipeline {
namespace {

/// Build a structured generation diagnostic.
[[nodiscard]] AssetIdentityGenerationDiagnostic MakeDiagnostic(
    std::string diagnosticId,
    std::string message,
    std::optional<std::size_t> inputIndex = std::nullopt,
    std::optional<std::size_t> previousIndex = std::nullopt,
    std::string assetKey = {},
    AssetId assetId = kInvalidAssetId)
{
    AssetIdentityGenerationDiagnostic diagnostic;
    diagnostic.diagnosticId = std::move(diagnosticId);
    diagnostic.message = std::move(message);
    diagnostic.inputIndex = inputIndex;
    diagnostic.previousIndex = previousIndex;
    diagnostic.assetKey = std::move(assetKey);
    diagnostic.assetId = assetId;
    return diagnostic;
}

} // namespace

AssetIdentityGenerationResult AssetIdentityGenerator::Generate(
    const std::vector<AssetIdentityInput>& inputs,
    const std::vector<AssetIdentityMapping>& previousMappings,
    const AssetIdentityGenerationOptions& options)
{
    (void)options;

    AssetIdentityGenerationResult result;
    std::map<std::string, AssetId> previousByKey;
    std::unordered_map<AssetId, std::string> previousKeyById;
    AssetId maxPreviousAssetId = kInvalidAssetId;

    for (std::size_t index = 0; index < previousMappings.size(); ++index)
    {
        const auto& mapping = previousMappings[index];
        if (mapping.assetKey.empty())
        {
            result.errors.push_back(MakeDiagnostic(
                AssetIdentityGenerationDiagnostics::InvalidAssetKey,
                "Previous asset identity mapping contains an empty AssetKey.",
                std::nullopt,
                index));
            continue;
        }

        if (mapping.assetId == kInvalidAssetId)
        {
            result.errors.push_back(MakeDiagnostic(
                AssetIdentityGenerationDiagnostics::InvalidAssetId,
                "Previous asset identity mapping contains reserved AssetId 0.",
                std::nullopt,
                index,
                mapping.assetKey,
                mapping.assetId));
            continue;
        }

        const auto previousKey = previousByKey.find(mapping.assetKey);
        if (previousKey != previousByKey.end())
        {
            result.errors.push_back(MakeDiagnostic(
                AssetIdentityGenerationDiagnostics::DuplicatePreviousAssetKey,
                "Previous asset identity mappings contain a duplicate AssetKey.",
                std::nullopt,
                index,
                mapping.assetKey,
                mapping.assetId));
            continue;
        }

        const auto previousId = previousKeyById.find(mapping.assetId);
        if (previousId != previousKeyById.end())
        {
            result.errors.push_back(MakeDiagnostic(
                AssetIdentityGenerationDiagnostics::DuplicateAssetId,
                "Previous asset identity mappings contain a duplicate AssetId.",
                std::nullopt,
                index,
                mapping.assetKey,
                mapping.assetId));
            continue;
        }

        previousByKey.emplace(mapping.assetKey, mapping.assetId);
        previousKeyById.emplace(mapping.assetId, mapping.assetKey);
        maxPreviousAssetId = std::max(maxPreviousAssetId, mapping.assetId);
    }

    std::map<std::string, AssetId> currentMappings;
    std::unordered_set<std::string> seenInputKeys;
    std::vector<std::string> newAssetKeys;

    for (std::size_t index = 0; index < inputs.size(); ++index)
    {
        const auto& input = inputs[index];
        if (input.assetKey.empty())
        {
            result.errors.push_back(MakeDiagnostic(
                AssetIdentityGenerationDiagnostics::InvalidAssetKey,
                "Asset identity input contains an empty AssetKey.",
                index,
                std::nullopt));
            continue;
        }

        const auto [_, inserted] = seenInputKeys.insert(input.assetKey);
        if (!inserted)
        {
            result.errors.push_back(MakeDiagnostic(
                AssetIdentityGenerationDiagnostics::DuplicateInputAssetKey,
                "Asset identity inputs contain a duplicate AssetKey.",
                index,
                std::nullopt,
                input.assetKey));
            continue;
        }

        const auto previous = previousByKey.find(input.assetKey);
        if (previous != previousByKey.end())
        {
            currentMappings.emplace(input.assetKey, previous->second);
        }
        else
        {
            newAssetKeys.push_back(input.assetKey);
        }
    }

    if (!result.Succeeded())
    {
        result.mappings.clear();
        return result;
    }

    std::sort(newAssetKeys.begin(), newAssetKeys.end());

    const auto maxAssetId = std::numeric_limits<AssetId>::max();
    if (!newAssetKeys.empty())
    {
        const auto requiredIds = static_cast<AssetId>(newAssetKeys.size());
        if (maxPreviousAssetId > maxAssetId - requiredIds)
        {
            result.errors.push_back(MakeDiagnostic(
                AssetIdentityGenerationDiagnostics::AssetIdOverflow,
                "Asset identity allocation would overflow the AssetId numeric range.",
                std::nullopt,
                std::nullopt,
                newAssetKeys.front(),
                maxPreviousAssetId));
            return result;
        }
    }

    AssetId nextAssetId = maxPreviousAssetId + 1;
    if (nextAssetId == kInvalidAssetId)
    {
        nextAssetId = 1;
    }

    for (const std::string& assetKey : newAssetKeys)
    {
        currentMappings.emplace(assetKey, nextAssetId);
        ++nextAssetId;
    }

    result.mappings.reserve(currentMappings.size());
    for (const auto& [assetKey, assetId] : currentMappings)
    {
        result.mappings.push_back(AssetIdentityMapping{
            .assetKey = assetKey,
            .assetId = assetId,
        });
    }

    return result;
}

} // namespace SagaAssetPipeline
