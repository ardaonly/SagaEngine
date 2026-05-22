/// @file PipelineResult.h
/// @brief Common result type for SagaPipeline orchestration steps.

#pragma once

#include "SagaPipeline/PipelineDiagnostic.h"

#include <vector>

namespace SagaPipeline
{

/// Result of running or planning a SagaPipeline step.
struct PipelineResult
{
    int exitCode = 0; ///< Child process exit code or pipeline usage/launch failure.
    std::vector<PipelineDiagnostic> diagnostics; ///< Diagnostics emitted by the step.

    /// Return true when the step completed without errors.
    [[nodiscard]] bool Succeeded() const noexcept;
};

} // namespace SagaPipeline
