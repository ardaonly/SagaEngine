/// @file FirstPlayableWorkflow.h
/// @brief Consolidated Product Shell first-playable diagnostics workflow.

#pragma once

#include "RuntimeEvidenceRunner.h"

#include <filesystem>
#include <iosfwd>

namespace SagaProduct
{

struct FirstPlayableWorkflowRequest
{
    RuntimeEvidenceRunRequest runtime;
    std::filesystem::path summaryPath;
};

struct FirstPlayableWorkflowResult
{
    EvidenceStatus status = EvidenceStatus::Incomplete;
    std::filesystem::path summaryPath;
    RuntimeEvidenceRunResult runtime;
};

[[nodiscard]] FirstPlayableWorkflowResult RunFirstPlayableWorkflow(
    const FirstPlayableWorkflowRequest& request,
    std::ostream& out,
    std::ostream& err);

} // namespace SagaProduct
