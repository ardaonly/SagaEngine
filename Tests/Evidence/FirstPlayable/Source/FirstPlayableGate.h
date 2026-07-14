/// @file FirstPlayableGate.h
/// @brief Release-candidate gate aggregation for first-playable evidence.
#pragma once

#include "FirstPlayable/FirstPlayableManualEvidence.h"
#include "FirstPlayable/FirstPlayablePublicClaimAudit.h"
#include "FirstPlayable/FirstPlayableWorkspacePolicy.h"
#include "FirstPlayable/RuntimeEvidenceRunner.h"
#include "VisualBlocks/VisualBlocksDescriptor.h"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace SagaProduct
{
enum class FirstPlayableGateStatus
{
    Accepted,
    AcceptedWithManualEvidencePending,
    Rejected,
    Incomplete,
};
[[nodiscard]] const char* ToString(FirstPlayableGateStatus status) noexcept;

struct FirstPlayableGateCheck
{
    std::string id;
    EvidenceStatus status = EvidenceStatus::Incomplete;
};

struct FirstPlayableGateResult
{
    FirstPlayableGateStatus status = FirstPlayableGateStatus::Incomplete;
    EvidenceStatus mandatoryProfiles = EvidenceStatus::Incomplete;
    FirstPlayableManualEvidenceResult manualEvidence;
    FirstPlayableWorkspacePolicyResult workspacePolicy;
    FirstPlayablePublicClaimAuditResult publicClaims;
    EvidenceStatus evidenceBundle = EvidenceStatus::Incomplete;
    std::vector<FirstPlayableGateCheck> checks;
    std::vector<std::string> nonClaims;
    std::vector<FirstPlayableDiagnostic> diagnostics;
    std::filesystem::path reportPath;
};

class FirstPlayableGate final
{
public:
    [[nodiscard]] static FirstPlayableGateResult Evaluate(
        const RuntimeEvidenceRunResult& runtime,
        const VisualBlocksDescriptorGenerationResult& descriptor,
        FirstPlayableWorkspacePolicyResult workspace,
        FirstPlayableManualEvidenceResult manual,
        FirstPlayablePublicClaimAuditResult claims,
        std::optional<FirstPlayableGateCheck> operatorCheck = std::nullopt);
};
} // namespace SagaProduct
