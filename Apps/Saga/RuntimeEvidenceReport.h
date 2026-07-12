/// @file RuntimeEvidenceReport.h
/// @brief Product-owned normalized view of runtime first-playable evidence.

#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace SagaProduct
{

enum class EvidenceStatus
{
    Passed,
    Warning,
    Failed,
    Incomplete,
    NotRequested,
    NotPresent,
    NoInputObserved,
    PendingManualEvidence,
    ExternalTestEvidence,
};

enum class EvidenceSeverity
{
    Info,
    Warning,
    Error,
};

struct FirstPlayableDiagnostic
{
    EvidenceSeverity severity = EvidenceSeverity::Error;
    std::string code;
    std::string message;
    std::optional<std::string> profileId;
    std::optional<std::filesystem::path> path;
    std::optional<std::string> actionHint;
    std::optional<std::string> blockId;
    std::optional<std::string> fieldPath;
};

struct RuntimeEvidenceCapabilities
{
    EvidenceStatus project = EvidenceStatus::NotPresent;
    EvidenceStatus scene = EvidenceStatus::NotPresent;
    EvidenceStatus runtimeSmoke = EvidenceStatus::NotPresent;
    EvidenceStatus scriptMetadata = EvidenceStatus::NotRequested;
    EvidenceStatus invocation = EvidenceStatus::NotRequested;
    EvidenceStatus lifecycle = EvidenceStatus::NotRequested;
    EvidenceStatus gameplay = EvidenceStatus::NotRequested;
    EvidenceStatus input = EvidenceStatus::NotPresent;
    EvidenceStatus render = EvidenceStatus::NotPresent;
};

struct RuntimeEvidenceReport
{
    EvidenceStatus status = EvidenceStatus::Incomplete;
    std::string projectId;
    std::string sceneId;
    RuntimeEvidenceCapabilities capabilities;
    std::vector<std::string> nonClaims;
    std::vector<FirstPlayableDiagnostic> diagnostics;
};

[[nodiscard]] const char* ToString(EvidenceStatus status) noexcept;
[[nodiscard]] const char* ToString(EvidenceSeverity severity) noexcept;

/// Parse and validate a runtime report for one known first-playable profile.
[[nodiscard]] RuntimeEvidenceReport ParseRuntimeEvidenceReport(
    const std::filesystem::path& reportPath,
    const std::string& profileId);

} // namespace SagaProduct
