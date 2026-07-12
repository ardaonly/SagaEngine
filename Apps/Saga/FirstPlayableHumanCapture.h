/// @file FirstPlayableHumanCapture.h
/// @brief Product Shell operator workflow for real-keyboard first-playable evidence.

#pragma once

#include "FirstPlayableWorkflow.h"

#include <chrono>
#include <cstdint>
#include <memory>

namespace SagaProduct
{

struct FirstPlayableHumanCaptureRequest
{
    FirstPlayableWorkflowRequest workflow;
    std::uint32_t frames = 600;
    std::chrono::milliseconds timeout{30000};
};

struct FirstPlayableHumanCaptureResult
{
    EvidenceStatus status = EvidenceStatus::Incomplete;
    FirstPlayableOperatorMode mode = FirstPlayableOperatorMode::Capture;
    FirstPlayableGateStatus gateStatus = FirstPlayableGateStatus::Incomplete;
    EvidenceProcessResult process;
    FirstPlayableManualEvidenceResult keyboardEvidence;
    FirstPlayableWorkflowResult workflow;
    std::filesystem::path reportPath;
    std::vector<FirstPlayableDiagnostic> diagnostics;
};

class FirstPlayableHumanCapture final
{
public:
    FirstPlayableHumanCapture();
    explicit FirstPlayableHumanCapture(std::unique_ptr<IEvidenceProcessRunner> runner);

    [[nodiscard]] FirstPlayableHumanCaptureResult Run(
        const FirstPlayableHumanCaptureRequest& request,
        std::ostream& out,
        std::ostream& err);

    [[nodiscard]] static EvidenceProcessRequest BuildProcessRequest(
        const FirstPlayableHumanCaptureRequest& request,
        const std::filesystem::path& reportPath);

private:
    std::unique_ptr<IEvidenceProcessRunner> m_runner;
};

} // namespace SagaProduct
