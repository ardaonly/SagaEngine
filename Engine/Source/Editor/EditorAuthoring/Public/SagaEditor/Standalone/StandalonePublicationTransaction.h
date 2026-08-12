// SPDX-License-Identifier: MPL-2.0

#pragma once

#include "SagaEditor/Documents/Document.h"

#include <filesystem>
#include <functional>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace SagaEditor
{
enum class StandalonePublicationRecoveryState
{
    None,
    CandidatePublished,
    PreviousRestored,
    Completed,
    FailedClosed,
};

enum class StandalonePublicationBoundary
{
    Prepared,
    PreviousMoveCompleted,
    PreviousMoved,
    CandidateMoveCompleted,
    CandidatePublished,
    Completed,
};

enum class StandalonePublicationCancellationOutcome
{
    None,
    BeforeCandidatePublication,
    PreviousOutputRestored,
    EvidenceRetained,
};

struct StandalonePublicationRecoveryResult
{
    StandalonePublicationRecoveryState state = StandalonePublicationRecoveryState::None;
    bool recovered = false;
    bool preservedPreviousOutput = false;
    bool retainedEvidence = false;
    bool terminalFailure = false;
    StandalonePublicationCancellationOutcome cancellationOutcome =
        StandalonePublicationCancellationOutcome::None;
    std::vector<DocumentDiagnostic> diagnostics;
    [[nodiscard]] bool Succeeded() const noexcept
    { return state != StandalonePublicationRecoveryState::FailedClosed; }
};

struct StandalonePublicationRequest
{
    std::filesystem::path stagingDirectory;
    std::filesystem::path candidateDirectory;
    std::filesystem::path destinationDirectory;
    std::string snapshotId;
    std::string manifestHash;
    std::function<bool()> publicationAllowed;
    /// Deterministic interruption hook used by recovery tests. Returning false
    /// retains the durable journal exactly at the reported boundary.
    std::function<bool(StandalonePublicationBoundary)> transitionAllowed;
};

struct StandalonePublicationResult
{
    bool published = false;
    bool cancelled = false;
    bool preservedPreviousOutput = false;
    StandalonePublicationRecoveryResult recovery;
    std::vector<DocumentDiagnostic> diagnostics;
};

/// Owns durable standalone publication and recovery. Only journals carrying
/// this authority's owner marker and paths contained by the output parent are
/// eligible for mutation; ambiguous or unverifiable state is retained.
class StandalonePublicationTransaction final
{
public:
    [[nodiscard]] static std::filesystem::path OwnedTransactionDirectory(
        const std::filesystem::path& outputDirectory,
        std::string_view transactionId);
    [[nodiscard]] static StandalonePublicationRecoveryResult RecoverOwned(
        const std::filesystem::path& outputDirectory);
    [[nodiscard]] static StandalonePublicationResult Publish(
        const StandalonePublicationRequest& request);
};
} // namespace SagaEditor
