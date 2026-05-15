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

/// User and workload input for resource-aware job planning.
struct JobRequest
{
    SchedulingPolicy policy          = SchedulingPolicy::Safe; ///< Scheduler policy.
    uint32_t         requestedJobs   = 0;                      ///< User-requested jobs; 0 means auto.
    bool             forceUnsafeJobs = false;                  ///< Bypass safety clamp for explicit jobs.
    bool             heavyDependency = false;                  ///< Apply package-build guardrails.
};

/// Resolved job plan with the detected limits that shaped the final count.
struct JobPlan
{
    HardwareReport hardware;                    ///< Host resources used for the calculation.
    uint32_t       requestedJobs   = 0;         ///< User-requested jobs; 0 means auto.
    uint32_t       detectedJobs    = 1;         ///< Policy-derived safe or performance job count.
    uint32_t       hardSafetyCap   = 1;         ///< CPU-bound safety ceiling.
    uint32_t       safetyLimit     = 1;         ///< Final safety ceiling after all guards.
    uint32_t       finalJobs       = 1;         ///< Job count passed to backend tools.
    bool           clamped         = false;     ///< True when requested jobs were reduced.
    bool           forceUnsafeJobs = false;     ///< True when explicit jobs bypassed safety.
    bool           heavyDependency = false;     ///< True when heavy dependency guardrails applied.
};

class BuildScheduler
{
public:
    BuildScheduler()                             = delete;
    BuildScheduler(const BuildScheduler&)        = delete;
    BuildScheduler& operator=(const BuildScheduler&) = delete;

    /// Resolve backend job count for the current machine.
    [[nodiscard]] static JobPlan PlanJobs(const JobRequest& request);

    /// Resolve backend job count for a supplied hardware report.
    [[nodiscard]] static JobPlan PlanJobsForReport(const HardwareReport& report, const JobRequest& request);

    /// Calculate the optimal number of parallel jobs for the current machine.
    /// @param policy The scheduling policy to apply.
    /// @param userOverride If > 0, this value is treated as a request and clamped unless unsafe mode is used.
    [[nodiscard]] static uint32_t CalculateJobs(SchedulingPolicy policy, uint32_t userOverride = 0);

    /// Convert a byte count to a human-readable GB string.
    [[nodiscard]] static double BytesToGB(uint64_t bytes);
};

} // namespace Forge
