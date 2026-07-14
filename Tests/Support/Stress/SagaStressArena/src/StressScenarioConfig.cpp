/// @file StressScenarioConfig.cpp
/// @brief Implements SagaStressArena scenario configuration validation.

#include "SagaStressArena/StressScenarioConfig.hpp"

#include <utility>

namespace SagaStressArena
{
namespace
{

void AddDiagnostic(std::vector<std::string>& diagnostics, std::string message)
{
    diagnostics.push_back(std::move(message));
}

[[nodiscard]] bool IsKnownScenario(const std::string& scenario)
{
    return scenario == kDirectZoneDiagnosticsSmokeScenario ||
           scenario == kDirectZonePacketChaosSmokeScenario;
}

} // namespace

StressScenarioConfig DefaultStressScenarioConfig()
{
    return StressScenarioConfig{};
}

StressScenarioConfigResult ResolveStressScenarioConfig(
    const StressScenarioConfig& input)
{
    StressScenarioConfigResult result;
    const StressTierDefinition tierDefinition = DefinitionForTier(input.tier);

    result.config.scenario = input.scenario;
    result.config.tier = input.tier;
    result.config.actors = input.actors.value_or(tierDefinition.defaultActors);
    result.config.ticks = input.ticks.value_or(tierDefinition.defaultTicks);
    result.config.maxDurationSec =
        input.maxDurationSec.value_or(tierDefinition.defaultMaxDurationSec);
    result.config.chaosConfig = input.chaosConfig;
    result.config.reportDirectory = input.reportDirectory;
    result.config.failFast = input.failFast;
    result.config.jsonOutput = input.jsonOutput;
    result.config.manualOnly = tierDefinition.manualOnly;

    if (!IsKnownScenario(input.scenario))
    {
        AddDiagnostic(result.diagnostics, "Unknown stress scenario.");
    }
    if (result.config.actors == 0 ||
        result.config.actors > tierDefinition.maxActors)
    {
        AddDiagnostic(result.diagnostics, "Actor count exceeds tier bounds.");
    }
    if (result.config.ticks == 0 ||
        result.config.ticks > tierDefinition.maxTicks)
    {
        AddDiagnostic(result.diagnostics, "Tick count exceeds tier bounds.");
    }
    if (result.config.maxDurationSec == 0 ||
        result.config.maxDurationSec > tierDefinition.maxDurationSec)
    {
        AddDiagnostic(result.diagnostics, "Duration exceeds tier bounds.");
    }
    if (input.reportDirectory.empty())
    {
        AddDiagnostic(result.diagnostics, "Report directory must be non-empty.");
    }

    return result;
}

} // namespace SagaStressArena
