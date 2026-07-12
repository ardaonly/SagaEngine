/// @file FirstPlayableWorkflow.h
/// @brief Consolidated Product Shell first-playable diagnostics workflow.

#pragma once

#include "RuntimeEvidenceRunner.h"
#include "VisualBlocksDescriptor.h"
#include "FirstPlayableGate.h"

#include <filesystem>
#include <iosfwd>

namespace SagaProduct
{

struct FirstPlayableWorkflowRequest
{
    RuntimeEvidenceRunRequest runtime;
    std::filesystem::path summaryPath;
    std::optional<std::filesystem::path> keyboardReportPath;
};

struct FirstPlayableWorkflowResult
{
    EvidenceStatus status = EvidenceStatus::Incomplete;
    std::filesystem::path summaryPath;
    RuntimeEvidenceRunResult runtime;
    VisualBlocksDescriptorGenerationResult visualBlocksDescriptor;
    FirstPlayableGateResult gate;
    std::filesystem::path gatePath;
    std::filesystem::path sourceManifestPath;
    std::filesystem::path evidenceManifestPath;
};

[[nodiscard]] FirstPlayableWorkflowResult RunFirstPlayableWorkflow(
    const FirstPlayableWorkflowRequest& request,
    std::ostream& out,
    std::ostream& err);

} // namespace SagaProduct
