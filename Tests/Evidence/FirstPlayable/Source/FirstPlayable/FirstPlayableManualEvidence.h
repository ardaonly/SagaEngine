/// @file FirstPlayableManualEvidence.h
/// @brief Strict import of optional real-keyboard evidence.
#pragma once

#include "Reports/RuntimeEvidenceReport.h"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace SagaProduct
{
struct FirstPlayableEvidencePosition
{
    double x = 0.0;
    double y = 0.0;
};

struct FirstPlayableManualEvidenceResult
{
    EvidenceStatus status = EvidenceStatus::PendingManualEvidence;
    std::optional<std::filesystem::path> reportPath;
    std::string reportSha256;
    std::string source;
    bool realDeviceObserved = false;
    std::uint64_t framesWithInput = 0;
    std::optional<FirstPlayableEvidencePosition> initialPosition;
    std::optional<FirstPlayableEvidencePosition> finalPosition;
    double movementDistance = 0.0;
    std::vector<FirstPlayableDiagnostic> diagnostics;
};

class FirstPlayableManualEvidence final
{
public:
    [[nodiscard]] static FirstPlayableManualEvidenceResult Validate(
        const std::optional<std::filesystem::path>& reportPath);
};
} // namespace SagaProduct
