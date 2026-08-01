// SPDX-License-Identifier: MPL-2.0

#pragma once

#include "SagaEditor/Project/ProjectMutation.h"

#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace SagaEditor
{
enum class PackageModuleOperation
{
    Enable,
    Disable,
};

enum class DependencyIntentEditStatus
{
    Changed,
    NoChanges,
    Invalid,
};

struct DependencyIntentEditResult final
{
    DependencyIntentEditStatus status = DependencyIntentEditStatus::Invalid;
    std::string code;
    std::string message;
    std::string candidateBytes;
};

/// Performs a span edit of one dependency's modules array. JSON outside the
/// selected token is retained byte-for-byte apart from required CRLF-to-LF
/// normalization.
class DependencyIntentEditor final
{
public:
    [[nodiscard]] static DependencyIntentEditResult Edit(
        std::string_view manifestBytes, std::string_view packageId,
        std::string_view moduleId, PackageModuleOperation operation);
};

struct PackageCatalogModule final
{
    std::string packageId;
    std::string version;
    std::string module;
    bool enabled = false;
    std::string runtimeReadiness;
    std::vector<std::string> artifactKinds;
    std::vector<std::string> apiSurfaces;
    std::vector<std::string> blocksSurfaces;
    std::vector<std::string> capabilities;
    std::string license;
    std::string trust;
    std::string origin;
    bool offline = false;

    bool operator==(const PackageCatalogModule&) const = default;
};

class PackageStateCatalog final
{
public:
    [[nodiscard]] bool LoadInspection(std::string_view bytes,
        std::string& diagnostic);
    [[nodiscard]] const std::vector<PackageCatalogModule>& Modules() const noexcept
    { return modules_; }
    [[nodiscard]] const std::string& GraphHash() const noexcept
    { return graphHash_; }
    [[nodiscard]] const std::string& GenerationId() const noexcept
    { return generationId_; }
    [[nodiscard]] const PackageCatalogModule* Find(
        std::string_view packageId, std::string_view module) const noexcept;
    void Invalidate();

private:
    std::vector<PackageCatalogModule> modules_;
    std::string graphHash_;
    std::string generationId_;
};

struct PackageTransactionReview final
{
    std::string transactionId;
    PackageModuleOperation operation = PackageModuleOperation::Enable;
    std::string packageId;
    std::string module;
    std::string version;
    std::string baseIntentHash;
    std::string candidateIntentHash;
    std::string baseGraphHash;
    std::string candidateGraphHash;
    std::vector<std::string> moduleChanges;
    std::vector<std::string> surfaceChanges;
    std::vector<std::string> artifactChanges;
    std::vector<std::string> capabilities;
    std::string license;
    std::string trust;
    bool offline = false;
    std::vector<std::string> usageImpact;
    std::vector<std::string> refreshPlan;
    bool stale = false;
    bool blocked = false;

    void Normalize();
};

enum class ProjectReadinessState
{
    Ready,
    ArtifactsOutOfDate,
    ConformanceFailed,
    ToolingOutOfDate,
    Dirty,
    Mutating,
    Invalid,
    RecoveryRequired,
};

/// Single Qt-free authority for Play/Build and projection/package availability.
class ProjectReadinessSession final
{
public:
    void SetRecoveryRequired(bool value) noexcept;
    void SetInvalid(bool value) noexcept;
    void SetMutating(bool value) noexcept;
    void SetDirty(bool value) noexcept;
    void SetToolingOutOfDate(bool value) noexcept;
    void SetArtifactsOutOfDate(bool value) noexcept;
    void SetConformanceFailed(bool value) noexcept;
    [[nodiscard]] ProjectReadinessState State() const noexcept;
    [[nodiscard]] bool CanPlayOrBuild() const noexcept
    { return State() == ProjectReadinessState::Ready; }
    [[nodiscard]] bool PackagePaletteAvailable() const noexcept
    { return State() == ProjectReadinessState::Ready; }

private:
    bool recoveryRequired_ = false;
    bool invalid_ = false;
    bool mutating_ = false;
    bool dirty_ = false;
    bool toolingOutOfDate_ = false;
    bool artifactsOutOfDate_ = false;
    bool conformanceFailed_ = false;
};

struct PackageWorkflowCapture final
{
    std::filesystem::path projectRoot;
    std::filesystem::path manifestPath;
    std::filesystem::path lockPath;
    std::string manifestBytes;
    std::string lockBytes;
    std::string sourceInventoryHash;
    std::string generationId;
    std::string generationHash;
    std::string graphHash;
    std::string ownershipCatalogHash;
    std::string projectHash;
    std::string manifestHash;
    std::string lockHash;
    std::string generationManifestHash;
    std::string referenceInventoryHash;
    std::vector<std::pair<std::filesystem::path, std::string>> savedSources;
    std::vector<std::pair<std::filesystem::path, std::string>> references;
    bool derivedStateCurrent = true;
};

struct PackageUsageLocation final
{
    std::filesystem::path sourcePath;
    std::uint64_t startByte = 0;
    std::uint64_t endByte = 0;
    int startLine = 0;
    int startColumn = 0;
    int endLine = 0;
    int endColumn = 0;
    std::string containingType;
    std::string containingBehavior;
    std::string documentationId;
    std::string apiId;
    std::string surface;
    std::string module;
    std::string packageId;

    bool operator==(const PackageUsageLocation&) const = default;
};

struct PackageUsageAnalysis final
{
    bool passed = false;
    std::string code;
    std::string message;
    std::string sourceInventoryHash;
    std::string generationId;
    std::string generationHash;
    std::string graphHash;
    std::string ownershipCatalogHash;
    std::vector<std::string> usages;
    std::string projectHash;
    std::string lockHash;
    std::string generationManifestHash;
    std::string referenceInventoryHash;
    std::vector<PackageUsageLocation> locations;
};

struct PackageCandidateResolution final
{
    bool passed = false;
    std::string code;
    std::string message;
    std::string candidateLockBytes;
    std::string inspectionBytes;
    std::string candidateGraphHash;
};

enum class PackageHistoryIntent
{
    Forward,
    Undo,
    Redo,
};

struct PackageWorkflowPrepareRequest final
{
    PackageWorkflowCapture capture;
    PackageModuleOperation operation = PackageModuleOperation::Enable;
    std::string packageId;
    std::string module;
    PackageHistoryIntent historyIntent = PackageHistoryIntent::Forward;
    bool relevantBuffersDirty = false;
    std::function<PackageWorkflowCapture()> recapture;
    std::function<PackageCandidateResolution(
        const std::filesystem::path& stagedManifest,
        const std::filesystem::path& stagingRoot)> resolveCandidate;
    std::function<PackageUsageAnalysis(
        std::string_view packageId, std::string_view module,
        const std::filesystem::path& stagingRoot)> analyzeUsage;
};

enum class PackageWorkflowStatus
{
    Prepared,
    NoChanges,
    Applied,
    Cancelled,
    ToolingOutOfDate,
    Rejected,
};

struct PackageWorkflowResult final
{
    PackageWorkflowStatus status = PackageWorkflowStatus::Rejected;
    std::string code;
    std::string message;
    std::optional<PackageTransactionReview> review;
};

struct PackageRefreshResult final
{
    bool passed = false;
    std::string code;
    std::string message;
    std::string inspectionBytes;
};

/// Stateful, Qt-free semantic package transaction authority. Tool execution is
/// injected by the product adapter, while lease, review freshness, canonical
/// publication, history, and readiness transitions remain centralized here.
class PackageModuleWorkflow final
{
public:
    PackageModuleWorkflow(ProjectReadinessSession& readiness,
        PackageStateCatalog& catalog);
    ~PackageModuleWorkflow();

    [[nodiscard]] PackageWorkflowResult Prepare(
        const PackageWorkflowPrepareRequest& request);
    [[nodiscard]] PackageWorkflowResult Apply(
        std::string_view transactionId,
        const std::function<PackageWorkflowCapture()>& recapture,
        const std::function<PackageRefreshResult()>& refresh);
    [[nodiscard]] PackageWorkflowResult Cancel();
    [[nodiscard]] PackageWorkflowResult RetryRefresh(
        const std::function<PackageRefreshResult()>& refresh);
    [[nodiscard]] bool Active() const noexcept;
    [[nodiscard]] const std::vector<PackageTransactionReview>& History() const noexcept;
    [[nodiscard]] std::optional<PackageTransactionReview> UndoIntent() const;
    [[nodiscard]] std::optional<PackageTransactionReview> RedoIntent() const;

private:
    struct ActiveReview;
    [[nodiscard]] PackageWorkflowResult FinishRefresh(
        const PackageRefreshResult& refresh, PackageWorkflowStatus success);
    void ReleaseActive(bool removeStaging);

    ProjectReadinessSession& readiness_;
    PackageStateCatalog& catalog_;
    std::unique_ptr<ActiveReview> active_;
    std::vector<PackageTransactionReview> history_;
    std::size_t historyCursor_ = 0;
    bool refreshPending_ = false;
    std::string lastAppliedTransactionId_;
};
}
