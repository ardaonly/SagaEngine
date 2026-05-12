/// @file BuildScheduler.h
/// @brief Resource-aware job scaling logic.

#pragma once

#include "Hardware.h"
#include <cstdint>

namespace Forge
{

/// Strategy for calculating parallel jobs.
enum class SchedulingPolicy
{
    Safe,       ///< Prioritize stability, conservative RAM usage.
    Aggressive, ///< Use all available cores regardless of RAM (current behavior).
    MaxPerformance ///< Balanced high performance.
};

class BuildScheduler
{
public:
    BuildScheduler()                             = delete;
    BuildScheduler(const BuildScheduler&)        = delete;
    BuildScheduler& operator=(const BuildScheduler&) = delete;

    /// Calculate the optimal number of parallel jobs for the current machine.
    /// @param policy The scheduling policy to apply.
    /// @param userOverride If > 0, this value is returned (user knows best).
    [[nodiscard]] static uint32_t CalculateJobs(SchedulingPolicy policy, uint32_t userOverride = 0);

    /// Convert a byte count to a human-readable GB string.
    [[nodiscard]] static double BytesToGB(uint64_t bytes);
};

} // namespace Forge
