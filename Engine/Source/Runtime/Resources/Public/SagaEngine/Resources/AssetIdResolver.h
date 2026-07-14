/// @file AssetIdResolver.h
/// @brief Explicit AssetKey to AssetId resolution boundary for runtime assets.

#pragma once

#include "SagaEngine/Resources/StreamingRequest.h"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace SagaEngine::Resources {

/// Canonical manifest-facing asset identity.
using AssetKey = std::string;

/// One resolved runtime identity pair.
struct AssetIdResolution
{
    AssetKey assetKey; ///< Canonical manifest-facing asset key.
    AssetId  assetId = kInvalidAssetId; ///< Runtime hot-path numeric asset id.
};

/// Structured diagnostic emitted while resolving asset identities.
struct AssetIdResolutionDiagnostic
{
    std::string diagnosticId;              ///< Stable Runtime.AssetIdentity.* diagnostic id.
    std::string message;                   ///< Human-readable diagnostic message.
    AssetKey assetKey;                     ///< Asset key involved in the diagnostic.
    AssetId assetId = kInvalidAssetId;     ///< Resolved asset id when available.
    std::optional<std::size_t> assetIndex; ///< Input index when applicable.
};

/// Result of resolving a batch of asset keys.
struct AssetIdResolutionResult
{
    std::vector<AssetIdResolution> resolutions;        ///< Successful key/id pairs.
    std::vector<AssetIdResolutionDiagnostic> diagnostics; ///< Resolution failures.

    /// Return true when all requested keys resolved without diagnostics.
    [[nodiscard]] bool Succeeded() const noexcept { return diagnostics.empty(); }
};

/// Interface for explicit AssetKey to AssetId mappings.
class IAssetIdResolver
{
public:
    virtual ~IAssetIdResolver() = default;

    /// Resolve an asset key to a numeric runtime id.
    [[nodiscard]] virtual bool ResolveAssetId(
        std::string_view assetKey,
        AssetId& outAssetId) const = 0;
};

/// In-memory resolver used by tests and future manifest/package handoff code.
class StaticAssetIdResolver final : public IAssetIdResolver
{
public:
    /// Add or replace one explicit asset identity mapping.
    void AddMapping(AssetKey assetKey, AssetId assetId);

    /// Resolve an asset key from the configured mapping table.
    [[nodiscard]] bool ResolveAssetId(
        std::string_view assetKey,
        AssetId& outAssetId) const override;

private:
    std::unordered_map<AssetKey, AssetId> mappings_;
};

/// Batch helper for validating identity mappings before registry mutation.
class AssetIdResolver
{
public:
    /// Resolve all keys and report missing, invalid, or colliding ids.
    [[nodiscard]] static AssetIdResolutionResult ResolveAssetKeys(
        const std::vector<AssetKey>& assetKeys,
        const IAssetIdResolver& resolver);
};

namespace AssetIdResolverDiagnostics {

inline constexpr const char* InvalidAssetKey =
    "Runtime.AssetIdentity.InvalidAssetKey";
inline constexpr const char* MissingKey =
    "Runtime.AssetIdentity.MissingKey";
inline constexpr const char* InvalidAssetId =
    "Runtime.AssetIdentity.InvalidAssetId";
inline constexpr const char* DuplicateAssetKey =
    "Runtime.AssetIdentity.DuplicateAssetKey";
inline constexpr const char* DuplicateAssetId =
    "Runtime.AssetIdentity.DuplicateAssetId";

} // namespace AssetIdResolverDiagnostics

} // namespace SagaEngine::Resources
