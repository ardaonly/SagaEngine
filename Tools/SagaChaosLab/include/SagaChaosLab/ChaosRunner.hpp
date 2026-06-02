/// @file ChaosRunner.hpp
/// @brief Runs bounded SagaChaosLab profiles through SagaStressArena.

#pragma once

#include "SagaChaosLab/ChaosReport.hpp"

namespace SagaChaosLab
{

/// Execution options for a chaos profile run.
struct ChaosRunnerOptions
{
    bool allowManual = false;
};

/// Bounded profile runner around the direct/local SagaStressArena harness.
class ChaosRunner
{
public:
    [[nodiscard]] static ChaosRunResult Run(
        const ChaosProfile& profile,
        const ChaosRunnerOptions& options = {});
};

} // namespace SagaChaosLab
