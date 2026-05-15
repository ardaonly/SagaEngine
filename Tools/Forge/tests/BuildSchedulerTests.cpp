/// @file BuildSchedulerTests.cpp
/// @brief Deterministic coverage for Forge job planning guardrails.

#include "Forge/BuildScheduler.h"

#include <cstdlib>
#include <iostream>
#include <string>

namespace
{

constexpr uint64_t kGiB = 1024ull * 1024ull * 1024ull;

/// Return a hardware report with stable test resources.
Forge::HardwareReport MakeHardware(uint32_t cpuCores, uint64_t ramGiB)
{
    Forge::HardwareReport report;
    report.cpuCores      = cpuCores;
    report.totalRamBytes = ramGiB * kGiB;
    return report;
}

/// Check a resolved job count and print a readable failure.
bool ExpectJobs(const char* label, const Forge::JobPlan& plan, uint32_t expected)
{
    if (plan.finalJobs == expected)
        return true;

    std::cerr << "[BuildSchedulerTests] " << label
              << ": expected jobs=" << expected
              << " got=" << plan.finalJobs
              << " detected=" << plan.detectedJobs
              << " safety_limit=" << plan.safetyLimit
              << "\n";
    return false;
}

} // namespace

int main()
{
    bool ok = true;

    ok &= ExpectJobs(
        "safe default caps baseline to two jobs",
        Forge::BuildScheduler::PlanJobsForReport(
            MakeHardware(8, 16),
            {Forge::SchedulingPolicy::Safe, 0, false, false}),
        2);

    ok &= ExpectJobs(
        "explicit jobs are clamped by safety limit",
        Forge::BuildScheduler::PlanJobsForReport(
            MakeHardware(16, 64),
            {Forge::SchedulingPolicy::Safe, 8, false, false}),
        4);

    ok &= ExpectJobs(
        "low memory falls back to one job",
        Forge::BuildScheduler::PlanJobsForReport(
            MakeHardware(4, 2),
            {Forge::SchedulingPolicy::Safe, 0, false, false}),
        1);

    ok &= ExpectJobs(
        "heavy dependency caps install jobs at two",
        Forge::BuildScheduler::PlanJobsForReport(
            MakeHardware(16, 64),
            {Forge::SchedulingPolicy::Safe, 4, false, true}),
        2);

    ok &= ExpectJobs(
        "force unsafe explicit jobs bypass clamp",
        Forge::BuildScheduler::PlanJobsForReport(
            MakeHardware(16, 64),
            {Forge::SchedulingPolicy::Safe, 8, true, true}),
        8);

    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
