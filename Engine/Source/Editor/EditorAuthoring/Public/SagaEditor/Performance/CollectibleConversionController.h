// SPDX-License-Identifier: MPL-2.0

#pragma once

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace SagaEditor
{
enum class CollectibleConversionState
{
    Ready,
    AwaitingReview,
    Applied,
    Reversed,
    DetachedRequiresReview,
    Rejected,
};

struct CollectibleConversionCapture final
{
    std::filesystem::path projectRoot;
    std::filesystem::path projectFile;
    std::filesystem::path lockFile;
    std::filesystem::path sourceFile;
    std::filesystem::path provenanceFile;
    std::string projectGeneration;
    std::string packageGraphHash;
    std::string sourceRevision;
};

struct CollectibleConversionPlan final
{
    std::string operationId;
    std::string sourceHash;
    std::string candidateSourceHash;
    std::string projectHash;
    std::string candidateProjectHash;
    std::string lockHash;
    std::string candidateLockHash;
    std::string provenanceHash;
    std::string candidateProvenance;
    std::string reversePatch;
    std::string projectGeneration;
    std::string packageGraphHash;
    std::string sourceRevision;
    std::string targetPackageId;
    std::string targetPackageVersion;
    std::vector<std::string> supportedRegionChecksums;
    std::string deterministicDiff;
};

struct CollectibleConversionReview final
{
    CollectibleConversionState state = CollectibleConversionState::Rejected;
    std::string code;
    std::string message;
    std::optional<CollectibleConversionPlan> plan;
};

/// Qt-free conversion authority. Package resolution supplies already-verified
/// project and lock candidates; this controller binds them to a reviewed source
/// and provenance update in one recoverable project mutation transaction.
class CollectibleConversionController final
{
public:
    CollectibleConversionController();
    ~CollectibleConversionController();
    [[nodiscard]] CollectibleConversionReview Prepare(
        const CollectibleConversionCapture& capture,
        std::string candidateProject,
        std::string candidateLock,
        std::string candidateSource,
        std::string targetPackageVersion);
    [[nodiscard]] CollectibleConversionReview Apply();
    [[nodiscard]] CollectibleConversionReview Reverse(
        const CollectibleConversionCapture& capture);
    [[nodiscard]] CollectibleConversionReview Cancel();
    [[nodiscard]] CollectibleConversionState State() const noexcept { return state_; }

private:
    struct Active;
    CollectibleConversionState state_ = CollectibleConversionState::Ready;
    std::unique_ptr<Active> active_;
};
}
