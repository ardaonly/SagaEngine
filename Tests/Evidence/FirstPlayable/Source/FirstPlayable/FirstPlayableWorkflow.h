/// @file FirstPlayableWorkflow.h
/// @brief Consolidated Product Shell first-playable diagnostics workflow.

#pragma once

#include "FirstPlayable/RuntimeEvidenceRunner.h"
#include "ProductIntegration/VisualBlocksDescriptor.h"
#include "FirstPlayable/FirstPlayableGate.h"

#include <filesystem>
#include <iosfwd>
#include <optional>

namespace SagaProduct
{

enum class FirstPlayableOperatorMode
{
    Capture,
    Import,
};

struct FirstPlayableOperatorEvidenceInput
{
    FirstPlayableOperatorMode mode = FirstPlayableOperatorMode::Capture;
    EvidenceStatus status = EvidenceStatus::Incomplete;
    std::optional<EvidenceProcessResult> process;
    std::filesystem::path originalReportPath;
    std::filesystem::path captureReportPath;
    std::filesystem::path stdoutPath;
    std::filesystem::path stderrPath;
    std::vector<FirstPlayableDiagnostic> diagnostics;
};

struct FirstPlayableWorkflowRequest
{
    RuntimeEvidenceRunRequest runtime;
    std::filesystem::path summaryPath;
    std::optional<std::filesystem::path> keyboardReportPath;
    std::optional<FirstPlayableWorkspacePolicySession> workspaceSession;
    std::optional<FirstPlayableOperatorEvidenceInput> operatorEvidence;
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
