/// @file AssetManifest.hpp
/// @brief Defines runtime-ready asset manifest value types.

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace SagaEngine::Assets
{

/// Runtime asset category accepted by packaged asset manifests.
enum class AssetKind : std::uint8_t
{
    Mesh = 0,
    Texture,
    Shader,
    Audio,
    Animation,
    Material,
};

/// Convert a manifest kind token into a runtime asset kind.
[[nodiscard]] std::optional<AssetKind> TryParseAssetKind(std::string_view value) noexcept;

/// Return the stable manifest token for a runtime asset kind.
[[nodiscard]] std::string_view ToString(AssetKind kind) noexcept;

/// Runtime-facing reference to one cooked asset artifact.
struct AssetManifestEntry
{
    std::string id;                                  ///< Stable asset id from the manifest.
    AssetKind kind;                                  ///< Runtime category used by consumers.
    std::string path;                                ///< Manifest-relative or package-relative artifact path.
    std::optional<std::string> hash;                 ///< Optional integrity hash reserved for later validation.
    std::vector<std::string> dependencies;           ///< Optional asset ids required before this asset is usable.
    std::optional<std::uint64_t> memoryEstimateBytes; ///< Optional residency budget hint.
    std::optional<std::string> streamingGroup;       ///< Optional group used by streaming policy.
    std::optional<std::string> platform;             ///< Optional target platform token.
    std::optional<std::string> profile;              ///< Optional build/profile token.
};

/// Parsed runtime asset manifest consumed by package startup validation.
struct AssetManifest
{
    std::uint32_t schemaVersion = 0;             ///< Manifest schema version accepted by the loader.
    std::vector<AssetManifestEntry> assets;      ///< Valid asset references parsed from the file.
};

} // namespace SagaEngine::Assets
