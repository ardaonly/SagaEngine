/// @file ArtifactKind.hpp
/// @brief Defines runtime-consumable artifact manifest kind tags.

#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

namespace SagaEngine::Artifacts
{

/// Artifact category consumed by runtime and server package loaders.
enum class ArtifactKind : std::uint8_t
{
    Graph = 0,
    Script,
    Asset,
    Data,
    Manifest,
    DiagnosticReport,
};

/// Convert a manifest kind token into a runtime artifact kind.
[[nodiscard]] std::optional<ArtifactKind> TryParseArtifactKind(std::string_view value) noexcept;

/// Return the stable manifest token for a runtime artifact kind.
[[nodiscard]] std::string_view ToString(ArtifactKind kind) noexcept;

} // namespace SagaEngine::Artifacts
