/// @file StressReportPaths.cpp
/// @brief Implements deterministic SagaStressArena report artifact paths.

#include "SagaStressArena/StressReportPaths.hpp"

namespace SagaStressArena
{

StressReportPaths BuildStressReportPaths(
    const std::filesystem::path& reportDirectory)
{
    StressReportPaths paths;
    paths.operationalSnapshot =
        reportDirectory / "direct_zone_stress_operational_snapshot.json";
    paths.reliabilityReport =
        reportDirectory / "direct_zone_stress_reliability_report.json";
    paths.lifetimeLeaks =
        reportDirectory / "direct_zone_stress_lifetime_leaks.json";
    return paths;
}

} // namespace SagaStressArena
