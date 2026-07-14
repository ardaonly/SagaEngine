/// @file StressScenarioConfig.hpp
/// @brief Defines SagaStressArena scenario configuration and validation.

#pragma once

#include "SagaStressArena/StressTier.hpp"

#include "SagaServer/Networking/Core/NetworkChaosLayer.h"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace SagaStressArena
{

inline constexpr const char* kDirectZoneDiagnosticsSmokeScenario =
    "direct_zone_diagnostics_smoke";
inline constexpr const char* kDirectZonePacketChaosSmokeScenario =
    "direct_zone_packet_chaos_smoke";

/// Caller-supplied stress scenario configuration.
struct StressScenarioConfig
{
    std::string scenario = kDirectZoneDiagnosticsSmokeScenario; ///< Scenario token.
    StressTier tier = StressTier::Smoke;                        ///< Selected tier.
    std::optional<std::uint32_t> actors;                        ///< Actor override.
    std::optional<std::uint32_t> ticks;                         ///< Tick override.
    std::optional<std::uint32_t> maxDurationSec;                ///< Wall-time cap override.
    std::optional<SagaServer::Networking::NetworkChaosConfig>
        chaosConfig;                                             ///< Optional packet chaos policy.
    std::filesystem::path reportDirectory =
        "diagnostics/reports/saga_stress_arena";                ///< Report directory.
    bool failFast = false;                                      ///< Stop on first write failure.
    bool jsonOutput = false;                                    ///< CLI should emit JSON.
};

/// Fully resolved stress scenario configuration.
struct ResolvedStressScenarioConfig
{
    std::string scenario;                       ///< Scenario token.
    StressTier tier = StressTier::Smoke;        ///< Selected tier.
    std::uint32_t actors = 0;                   ///< Effective actor count.
    std::uint32_t ticks = 0;                    ///< Effective tick count.
    std::uint32_t maxDurationSec = 0;           ///< Effective wall-time cap.
    std::optional<SagaServer::Networking::NetworkChaosConfig>
        chaosConfig;                            ///< Effective packet chaos policy override.
    std::filesystem::path reportDirectory;      ///< Effective report directory.
    bool failFast = false;                      ///< Stop on first write failure.
    bool jsonOutput = false;                    ///< CLI should emit JSON.
    bool manualOnly = false;                    ///< True when execution is blocked.
};

/// Result from resolving and validating a stress scenario configuration.
struct StressScenarioConfigResult
{
    ResolvedStressScenarioConfig config;        ///< Resolved configuration.
    std::vector<std::string> diagnostics;       ///< Deterministic validation diagnostics.

    /// Return true when the configuration is valid.
    [[nodiscard]] bool Succeeded() const noexcept
    {
        return diagnostics.empty();
    }
};

/// Return the default safe local stress scenario configuration.
[[nodiscard]] StressScenarioConfig DefaultStressScenarioConfig();

/// Resolve overrides and enforce tier safety caps.
[[nodiscard]] StressScenarioConfigResult ResolveStressScenarioConfig(
    const StressScenarioConfig& input);

} // namespace SagaStressArena
