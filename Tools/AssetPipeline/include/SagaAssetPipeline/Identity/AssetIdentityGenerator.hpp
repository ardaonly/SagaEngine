/// @file AssetIdentityGenerator.hpp
/// @brief Defines deterministic AssetKey to AssetId allocation for asset identity manifests.

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace SagaAssetPipeline {

using AssetId = std::uint64_t;

inline constexpr AssetId kInvalidAssetId = 0;

/// Current package asset key that needs a stable numeric identity.
struct AssetIdentityInput
{
    std::string assetKey; ///< Canonical package-facing asset key.
};

/// Stable mapping emitted into an asset identity manifest.
struct AssetIdentityMapping
{
    std::string assetKey;                ///< Canonical package-facing asset key.
    AssetId     assetId = kInvalidAssetId; ///< Stable runtime numeric id.
};

/// Reserved option bag for future explicit policy settings.
struct AssetIdentityGenerationOptions
{
};

/// Structured deterministic failure produced while allocating asset identities.
struct AssetIdentityGenerationDiagnostic
{
    std::string diagnosticId;                     ///< Stable AssetPipeline.AssetIdentity.* diagnostic id.
    std::string message;                          ///< Human-readable diagnostic detail.
    std::optional<std::size_t> inputIndex;        ///< Current input index when applicable.
    std::optional<std::size_t> previousIndex;     ///< Previous mapping index when applicable.
    std::string assetKey;                         ///< Asset key involved in the diagnostic.
    AssetId assetId = kInvalidAssetId;            ///< Asset id involved in the diagnostic.
};

/// Complete result for one identity allocation pass.
struct AssetIdentityGenerationResult
{
    std::vector<AssetIdentityMapping> mappings;             ///< Deterministic output mappings when successful.
    std::vector<AssetIdentityGenerationDiagnostic> errors;  ///< Validation/allocation failures.

    /// Return true when generation completed without diagnostics.
    [[nodiscard]] bool Succeeded() const noexcept { return errors.empty(); }
};

/// Generates deterministic AssetId mappings without reading or writing manifests.
class AssetIdentityGenerator
{
public:
    /// Allocate identities for current asset keys using a previous valid mapping set as source of truth.
    [[nodiscard]] static AssetIdentityGenerationResult Generate(
        const std::vector<AssetIdentityInput>& inputs,
        const std::vector<AssetIdentityMapping>& previousMappings,
        const AssetIdentityGenerationOptions& options = {});
};

namespace AssetIdentityGenerationDiagnostics {

inline constexpr const char* InvalidAssetKey =
    "AssetPipeline.AssetIdentity.InvalidAssetKey";
inline constexpr const char* DuplicateInputAssetKey =
    "AssetPipeline.AssetIdentity.DuplicateInputAssetKey";
inline constexpr const char* DuplicatePreviousAssetKey =
    "AssetPipeline.AssetIdentity.DuplicatePreviousAssetKey";
inline constexpr const char* InvalidAssetId =
    "AssetPipeline.AssetIdentity.InvalidAssetId";
inline constexpr const char* DuplicateAssetId =
    "AssetPipeline.AssetIdentity.DuplicateAssetId";
inline constexpr const char* AssetIdOverflow =
    "AssetPipeline.AssetIdentity.AssetIdOverflow";

} // namespace AssetIdentityGenerationDiagnostics

} // namespace SagaAssetPipeline
