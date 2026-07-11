/// @file StarterArenaSimulation.cpp
/// @brief Deterministic app-local simulation kernel shared by StarterArena modes.

#include "StarterArenaSimulation.h"

#include <algorithm>

namespace SagaRuntimeApp
{

StarterArenaSimulationState MakeStarterArenaSimulation(const StarterArenaScene& scene) noexcept
{
    StarterArenaSimulationState state;
    state.position = scene.playerSpawn;
    state.bounds = scene.bounds;
    return state;
}

void StepStarterArenaSimulation(StarterArenaSimulationState& state,
                                StarterArenaVec2 input,
                                double fixedDtSeconds) noexcept
{
    const double nextX = state.position.x + input.x * fixedDtSeconds;
    const double nextY = state.position.y + input.y * fixedDtSeconds;
    const double clampedX = std::clamp(nextX, state.bounds.minX, state.bounds.maxX);
    const double clampedY = std::clamp(nextY, state.bounds.minY, state.bounds.maxY);
    if (clampedX != nextX || clampedY != nextY)
    {
        ++state.clampCount;
    }
    state.position = {clampedX, clampedY};
    ++state.ticks;
}

} // namespace SagaRuntimeApp
