/// @file FirstPlayableGate.cpp
#include "FirstPlayableGate.h"

namespace SagaProduct
{
namespace
{
void GateError(FirstPlayableGateResult& result, std::string message,
               const std::string& profile = {})
{
    FirstPlayableDiagnostic diagnostic;
    diagnostic.code = "ProductShell.FirstPlayable.GateMandatoryProfileFailed";
    diagnostic.message = std::move(message);
    if (!profile.empty()) diagnostic.profileId = profile;
    result.diagnostics.push_back(std::move(diagnostic));
}
bool PreparationFailureIsExecution(const RuntimeEvidenceRunResult& runtime)
{
    for (const auto& diagnostic : runtime.diagnostics)
        if (diagnostic.code == "ProductShell.FirstPlayable.ProcessExitFailed" ||
            diagnostic.code == "ProductShell.FirstPlayable.ProcessTimedOut") return true;
    return false;
}
} // namespace

const char* ToString(FirstPlayableGateStatus status) noexcept
{
    switch (status)
    {
        case FirstPlayableGateStatus::Accepted: return "Accepted";
        case FirstPlayableGateStatus::AcceptedWithManualEvidencePending:
            return "AcceptedWithManualEvidencePending";
        case FirstPlayableGateStatus::Rejected: return "Rejected";
        case FirstPlayableGateStatus::Incomplete: return "Incomplete";
    }
    return "Incomplete";
}

FirstPlayableGateResult FirstPlayableGate::Evaluate(
    const RuntimeEvidenceRunResult& runtime,
    const VisualBlocksDescriptorGenerationResult& descriptor,
    FirstPlayableWorkspacePolicyResult workspace,
    FirstPlayableManualEvidenceResult manual,
    FirstPlayablePublicClaimAuditResult claims)
{
    FirstPlayableGateResult result;
    result.workspacePolicy = std::move(workspace);
    result.manualEvidence = std::move(manual);
    result.publicClaims = std::move(claims);
    result.nonClaims = FirstPlayablePublicClaimAudit::CanonicalNonClaims();
    const bool preflightIncomplete = !runtime.prepared &&
        !PreparationFailureIsExecution(runtime);
    result.evidenceBundle = preflightIncomplete ? EvidenceStatus::Incomplete :
                                                 EvidenceStatus::Passed;
    bool mandatory = runtime.prepared && runtime.profiles.size() == 5;
    for (const auto& profile : runtime.profiles)
    {
        result.diagnostics.insert(result.diagnostics.end(),
            profile.diagnostics.begin(), profile.diagnostics.end());
        if (!preflightIncomplete && profile.status != EvidenceStatus::Passed)
        {
            mandatory = false;
            GateError(result, "mandatory runtime profile failed", ToString(profile.profile));
        }
    }
    if (!preflightIncomplete && descriptor.status != EvidenceStatus::Passed)
    {
        mandatory = false;
        GateError(result, "mandatory Visual Blocks descriptor profile failed",
                  kVisualBlocksDescriptorProfileId);
    }
    result.mandatoryProfiles = preflightIncomplete ? EvidenceStatus::Incomplete :
        (mandatory ? EvidenceStatus::Passed : EvidenceStatus::Failed);
    result.checks = {
        {"workspace-policy", result.workspacePolicy.status},
        {"source-integrity", result.workspacePolicy.sourceIntegrity},
        {"public-claims", result.publicClaims.status},
        {"evidence-bundle", result.evidenceBundle},
    };
    result.diagnostics.insert(result.diagnostics.end(), runtime.diagnostics.begin(),
                              runtime.diagnostics.end());
    result.diagnostics.insert(result.diagnostics.end(), descriptor.diagnostics.begin(),
                              descriptor.diagnostics.end());
    result.diagnostics.insert(result.diagnostics.end(),
        result.workspacePolicy.diagnostics.begin(), result.workspacePolicy.diagnostics.end());
    result.diagnostics.insert(result.diagnostics.end(), result.publicClaims.diagnostics.begin(),
                              result.publicClaims.diagnostics.end());
    result.diagnostics.insert(result.diagnostics.end(), result.manualEvidence.diagnostics.begin(),
                              result.manualEvidence.diagnostics.end());

    if (preflightIncomplete)
        result.status = FirstPlayableGateStatus::Incomplete;
    else if (!mandatory || result.workspacePolicy.status != EvidenceStatus::Passed ||
        result.publicClaims.status != EvidenceStatus::Passed ||
        result.manualEvidence.status == EvidenceStatus::Failed)
        result.status = FirstPlayableGateStatus::Rejected;
    else if (result.manualEvidence.status == EvidenceStatus::PendingManualEvidence)
        result.status = FirstPlayableGateStatus::AcceptedWithManualEvidencePending;
    else result.status = FirstPlayableGateStatus::Accepted;
    return result;
}
} // namespace SagaProduct
