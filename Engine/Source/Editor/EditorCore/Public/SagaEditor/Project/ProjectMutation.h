// SPDX-License-Identifier: MPL-2.0

#pragma once

#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace SagaEditor
{
enum class ProjectMutationPhase
{
    Preparing,
    Resolved,
    Reviewed,
    Staging,
    CommitMarked,
    Publishing,
    Committed,
    Cancelled,
    RolledBack,
    RecoveryRequired,
};

enum class ProjectMutationRecoveryAction
{
    None,
    Discarded,
    RolledForward,
    Finalized,
    RecoveryRequired,
    UnsupportedJournal,
};

struct ProjectMutationFile final
{
    std::filesystem::path relativeTarget;
    std::filesystem::path stagedPath;
    std::string oldHash;
    std::string newHash;
    std::string oldIdentity;
    std::string stagedIdentity;
    bool published = false;
};

struct ProjectMutationOwner final
{
    std::string pid;
    std::string processIdentity;
    std::string nonce;
    std::string mutationKind;
    std::string projectHash;
    std::string acquiredAt;
};

struct ProjectMutationResult final
{
    bool succeeded = false;
    std::string code;
    std::string message;
};

struct ProjectMutationRecoveryResult final
{
    ProjectMutationRecoveryAction action = ProjectMutationRecoveryAction::None;
    std::filesystem::path journal;
    std::string code;
    std::string message;
};

// Owns one OS-backed writer lock for a canonical project root. Acquisition
// resolves symlinks, records process incarnation identity, and fails closed when
// the platform cannot provide the required lock/identity primitives.
class ProjectMutationLease final
{
public:
    ProjectMutationLease();
    ~ProjectMutationLease();
    ProjectMutationLease(ProjectMutationLease&&) noexcept;
    ProjectMutationLease& operator=(ProjectMutationLease&&) noexcept;
    ProjectMutationLease(const ProjectMutationLease&) = delete;
    ProjectMutationLease& operator=(const ProjectMutationLease&) = delete;

    [[nodiscard]] static std::pair<ProjectMutationLease, ProjectMutationResult>
        Acquire(const std::filesystem::path& projectRoot, std::string mutationKind);
    [[nodiscard]] bool Held() const noexcept;
    [[nodiscard]] bool Revalidate() const;
    [[nodiscard]] const std::filesystem::path& ProjectRoot() const noexcept;
    [[nodiscard]] const ProjectMutationOwner& Owner() const noexcept;

private:
    struct State;
    explicit ProjectMutationLease(std::unique_ptr<State> state);
    std::unique_ptr<State> state_;
};

// Versioned durable description of a canonical multi-file publication.
class ProjectMutationJournal final
{
public:
    static constexpr int CurrentSchemaVersion = 1;

    int schemaVersion = CurrentSchemaVersion;
    std::string transactionId;
    std::string operationId;
    std::string projectIdentity;
    std::string toolVersion;
    ProjectMutationOwner owner;
    ProjectMutationPhase phase = ProjectMutationPhase::Preparing;
    bool commitMarker = false;
    std::vector<ProjectMutationFile> files;

    [[nodiscard]] ProjectMutationResult Save(
        const std::filesystem::path& journalPath) const;
    [[nodiscard]] static std::pair<std::optional<ProjectMutationJournal>,
        ProjectMutationResult> Load(const std::filesystem::path& journalPath);
};

enum class ProjectMutationFaultPoint
{
    AfterStaging,
    AfterDurableJournal,
    AfterCommitMarker,
    AfterFirstPublication,
    AfterSecondPublication,
    BeforeFinalVerification,
};
using ProjectMutationFaultInjector =
    std::function<bool(ProjectMutationFaultPoint)>;

// Stages a reviewed canonical file set and publishes it with compare-and-swap
// checks. Cancellation is accepted only before the durable commit marker.
class ProjectMutationTransaction final
{
public:
    struct CandidateFile final
    {
        std::filesystem::path relativeTarget;
        std::string bytes;
    };

    ProjectMutationTransaction(ProjectMutationLease& lease,
        std::string operationId, std::string toolVersion,
        ProjectMutationFaultInjector injector = {});

    [[nodiscard]] ProjectMutationResult Stage(
        const std::vector<CandidateFile>& candidates);
    [[nodiscard]] ProjectMutationResult MarkCommit();
    [[nodiscard]] ProjectMutationResult Publish();
    [[nodiscard]] ProjectMutationResult Cancel();
    [[nodiscard]] const ProjectMutationJournal& Journal() const noexcept;
    [[nodiscard]] const std::filesystem::path& JournalPath() const noexcept;

private:
    [[nodiscard]] ProjectMutationResult Persist();
    [[nodiscard]] ProjectMutationResult RevalidateReviewedInventory(
        bool publicationStarted) const;
    [[nodiscard]] ProjectMutationResult EnterRecoveryRequired(
        std::string code, std::string message);
    [[nodiscard]] ProjectMutationResult FailBeforeCommit(
        std::string code, std::string message);
    ProjectMutationLease& lease_;
    ProjectMutationJournal journal_;
    std::filesystem::path transactionRoot_;
    std::filesystem::path journalPath_;
    ProjectMutationFaultInjector injector_;
};

// Scans local package journals and deterministically rolls forward only bytes
// already named by the old/new identity set. Divergence is preserved for manual
// recovery and unknown newer schemas are never rewritten.
class ProjectMutationRecovery final
{
public:
    [[nodiscard]] static std::vector<ProjectMutationRecoveryResult> Scan(
        const std::filesystem::path& projectRoot);
    [[nodiscard]] static ProjectMutationRecoveryResult Recover(
        const std::filesystem::path& projectRoot,
        const std::filesystem::path& journalPath);
};
}
