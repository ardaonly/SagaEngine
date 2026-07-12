/// @file FirstPlayableManualEvidence.h
/// @brief Strict import of optional real-keyboard evidence.
#pragma once

#include "RuntimeEvidenceReport.h"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace SagaProduct
{
struct FirstPlayableManualEvidenceResult
{
    EvidenceStatus status = EvidenceStatus::PendingManualEvidence;
    std::optional<std::filesystem::path> reportPath;
    std::string reportSha256;
    bool realDeviceObserved = false;
    std::uint64_t framesWithInput = 0;
    std::vector<FirstPlayableDiagnostic> diagnostics;
};

class FirstPlayableManualEvidence final
{
public:
    [[nodiscard]] static FirstPlayableManualEvidenceResult Validate(
        const std::optional<std::filesystem::path>& reportPath);
};
} // namespace SagaProduct
