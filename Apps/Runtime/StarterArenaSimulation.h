/// @file StarterArenaSimulation.h
/// @brief Deterministic app-local simulation kernel shared by StarterArena modes.

#pragma once

#include "StarterArenaSmoke.h"

#include <cstdint>

namespace SagaRuntimeApp
{

struct StarterArenaSimulationState
{
    StarterArenaVec2 position;
    StarterArenaBounds bounds;
    std::uint32_t ticks = 0;
    std::uint32_t clampCount = 0;
};

[[nodiscard]] StarterArenaSimulationState MakeStarterArenaSimulation(
    const StarterArenaScene& scene) noexcept;

void StepStarterArenaSimulation(StarterArenaSimulationState& state,
                                StarterArenaVec2 input,
                                double fixedDtSeconds) noexcept;

} // namespace SagaRuntimeApp
