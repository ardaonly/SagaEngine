/// @file PipelineResult.cpp
/// @brief Result helpers for SagaPipeline orchestration steps.

#include "SagaPipeline/PipelineResult.h"

namespace SagaPipeline
{

bool PipelineResult::Succeeded() const noexcept
{
    if (exitCode != 0)
    {
        return false;
    }

    for (const PipelineDiagnostic& diagnostic : diagnostics)
    {
        if (diagnostic.severity == PipelineDiagnosticSeverity::Error)
        {
            return false;
        }
    }
    return true;
}

} // namespace SagaPipeline
