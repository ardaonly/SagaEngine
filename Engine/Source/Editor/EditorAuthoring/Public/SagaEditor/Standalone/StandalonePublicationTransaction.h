// SPDX-License-Identifier: MPL-2.0

#pragma once

#include "SagaEditor/Documents/Document.h"

#include <filesystem>
#include <functional>
#include <map>
#include <string>
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

struct StandalonePublicationRecoveryResult
{
    StandalonePublicationRecoveryState state = StandalonePublicationRecoveryState::None;
    bool recovered = false;
    bool preservedPreviousOutput = false;
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
};

struct StandalonePublicationResult
{
    bool published = false;
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
    [[nodiscard]] static StandalonePublicationRecoveryResult RecoverOwned(
        const std::filesystem::path& outputDirectory);
    [[nodiscard]] static StandalonePublicationResult Publish(
        const StandalonePublicationRequest& request);
};
} // namespace SagaEditor
