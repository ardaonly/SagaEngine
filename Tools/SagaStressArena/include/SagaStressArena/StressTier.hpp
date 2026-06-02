/// @file StressTier.hpp
/// @brief Defines bounded SagaStressArena execution tiers.

#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

namespace SagaStressArena
{

/// Bounded stress tier selected for a local diagnostics scenario.
enum class StressTier : std::uint8_t
{
    Smoke,
    MiniSoak,
    Heavy,
};

/// Safety bounds and defaults for one stress tier.
struct StressTierDefinition
{
    StressTier tier = StressTier::Smoke;       ///< Tier represented by this definition.
    std::uint32_t defaultActors = 0;           ///< Default actor count.
    std::uint32_t defaultTicks = 0;            ///< Default tick count.
    std::uint32_t defaultMaxDurationSec = 0;   ///< Default wall-time cap.
    std::uint32_t maxActors = 0;               ///< Hard actor cap.
    std::uint32_t maxTicks = 0;                ///< Hard tick cap.
    std::uint32_t maxDurationSec = 0;          ///< Hard wall-time cap.
    bool manualOnly = false;                   ///< True when the tier must not execute in v1.
};

/// Return the stable string for a stress tier.
[[nodiscard]] const char* ToString(StressTier tier) noexcept;

/// Parse a stable stress tier token.
[[nodiscard]] std::optional<StressTier> ParseStressTier(
    std::string_view value) noexcept;

/// Return deterministic bounds for a stress tier.
[[nodiscard]] StressTierDefinition DefinitionForTier(StressTier tier) noexcept;

} // namespace SagaStressArena
