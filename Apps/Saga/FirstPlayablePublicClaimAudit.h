/// @file FirstPlayablePublicClaimAudit.h
/// @brief Focused public StarterArena claim audit.
#pragma once

#include "RuntimeEvidenceReport.h"

#include <filesystem>
#include <string>
#include <vector>

namespace SagaProduct
{
struct FirstPlayablePublicClaimAuditResult
{
    EvidenceStatus status = EvidenceStatus::Incomplete;
    std::vector<std::string> nonClaims;
    std::vector<FirstPlayableDiagnostic> diagnostics;
};

class FirstPlayablePublicClaimAudit final
{
public:
    [[nodiscard]] static FirstPlayablePublicClaimAuditResult Run(
        const std::filesystem::path& projectRoot);
    [[nodiscard]] static const std::vector<std::string>& CanonicalNonClaims();
};
} // namespace SagaProduct
