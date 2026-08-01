// SPDX-License-Identifier: MPL-2.0

#include "SagaEditor/Packages/PackageAuthoring.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <chrono>
#include <cctype>
#include <fstream>
#include <openssl/sha.h>
#include <iomanip>
#include <set>
#include <sstream>
#include <stdexcept>
#include <tuple>

namespace SagaEditor
{
namespace
{
using Json = nlohmann::json;

std::string NormalizeLf(std::string_view bytes)
{
    std::string normalized;
    normalized.reserve(bytes.size());
    for (std::size_t index = 0; index < bytes.size(); ++index)
    {
        if (bytes[index] == '\r' && index + 1 < bytes.size() &&
            bytes[index + 1] == '\n')
            continue;
        normalized.push_back(bytes[index]);
    }
    return normalized;
}

std::size_t SkipString(std::string_view bytes, std::size_t start)
{
    for (std::size_t index = start + 1; index < bytes.size(); ++index)
    {
        if (bytes[index] == '\\') ++index;
        else if (bytes[index] == '"') return index + 1;
    }
    return std::string_view::npos;
}

std::size_t Matching(std::string_view bytes, std::size_t start,
    char open, char close)
{
    int depth = 0;
    for (std::size_t index = start; index < bytes.size(); ++index)
    {
        if (bytes[index] == '"')
        {
            index = SkipString(bytes, index);
            if (index == std::string_view::npos) return index;
            --index;
            continue;
        }
        if (bytes[index] == open) ++depth;
        else if (bytes[index] == close && --depth == 0) return index;
    }
    return std::string_view::npos;
}

std::size_t FindProperty(std::string_view bytes, std::string_view name,
    std::size_t begin, std::size_t end)
{
    const auto needle = '"' + std::string(name) + '"';
    for (auto position = bytes.find(needle, begin);
         position != std::string_view::npos && position < end;
         position = bytes.find(needle, position + needle.size()))
    {
        auto cursor = position + needle.size();
        while (cursor < end &&
               std::isspace(static_cast<unsigned char>(bytes[cursor]))) ++cursor;
        if (cursor < end && bytes[cursor] == ':') return cursor + 1;
    }
    return std::string_view::npos;
}

template <typename T>
void SortUnique(std::vector<T>& values)
{
    std::sort(values.begin(), values.end());
    values.erase(std::unique(values.begin(), values.end()), values.end());
}

std::string Sha256(std::string_view bytes)
{
    unsigned char digest[SHA256_DIGEST_LENGTH]{};
    SHA256(reinterpret_cast<const unsigned char*>(bytes.data()), bytes.size(), digest);
    std::ostringstream output;
    output << "sha256:" << std::hex << std::setfill('0');
    for (const auto value : digest) output << std::setw(2) << static_cast<int>(value);
    return output.str();
}

std::string TransactionId()
{
    return "package-review-" + std::to_string(
        std::chrono::steady_clock::now().time_since_epoch().count());
}

bool SameCapture(const PackageWorkflowCapture& left,
    const PackageWorkflowCapture& right)
{
    return left.manifestBytes == right.manifestBytes &&
        left.lockBytes == right.lockBytes &&
        left.sourceInventoryHash == right.sourceInventoryHash &&
        left.generationId == right.generationId &&
        left.generationHash == right.generationHash &&
        left.graphHash == right.graphHash &&
        left.ownershipCatalogHash == right.ownershipCatalogHash &&
        left.projectHash == right.projectHash &&
        left.manifestHash == right.manifestHash &&
        left.lockHash == right.lockHash &&
        left.generationManifestHash == right.generationManifestHash &&
        left.referenceInventoryHash == right.referenceInventoryHash &&
        left.savedSources == right.savedSources &&
        left.references == right.references &&
        left.derivedStateCurrent == right.derivedStateCurrent;
}
}

DependencyIntentEditResult DependencyIntentEditor::Edit(
    std::string_view manifestBytes, std::string_view packageId,
    std::string_view moduleId, PackageModuleOperation operation)
{
    if (packageId.empty() || moduleId.empty())
        return {DependencyIntentEditStatus::Invalid,
            "Project.PackageTransaction.InvalidIntent",
            "Package and module identities are required.", {}};
    auto bytes = NormalizeLf(manifestBytes);
    Json document;
    try { document = Json::parse(bytes); }
    catch (const std::exception& exception)
    {
        return {DependencyIntentEditStatus::Invalid,
            "Project.PackageTransaction.InvalidManifest", exception.what(), {}};
    }
    if (!document.is_object() || !document.contains("dependencies") ||
        !document["dependencies"].is_array())
        return {DependencyIntentEditStatus::Invalid,
            "Project.PackageTransaction.InvalidManifest",
            "The project manifest requires a dependencies array.", {}};

    const auto dependenciesValue = FindProperty(
        bytes, "dependencies", 0, bytes.size());
    const auto arrayStart = dependenciesValue == std::string::npos
        ? std::string::npos : bytes.find('[', dependenciesValue);
    const auto arrayEnd = arrayStart == std::string::npos
        ? std::string::npos : Matching(bytes, arrayStart, '[', ']');
    if (arrayEnd == std::string::npos)
        return {DependencyIntentEditStatus::Invalid,
            "Project.PackageTransaction.IntentSpanMissing",
            "The dependency token span could not be located.", {}};

    std::size_t objectStart = std::string::npos;
    std::size_t objectEnd = std::string::npos;
    for (auto cursor = arrayStart + 1; cursor < arrayEnd;)
    {
        if (bytes[cursor] == '"')
        {
            cursor = SkipString(bytes, cursor);
            if (cursor == std::string::npos) break;
            continue;
        }
        if (bytes[cursor] != '{') { ++cursor; continue; }
        const auto end = Matching(bytes, cursor, '{', '}');
        if (end == std::string::npos) break;
        try
        {
            const auto dependency = Json::parse(
                bytes.substr(cursor, end - cursor + 1));
            if (dependency.value("packageId", "") == packageId)
            {
                objectStart = cursor;
                objectEnd = end + 1;
                break;
            }
        }
        catch (...) {}
        cursor = end + 1;
    }
    if (objectStart == std::string::npos)
        return {DependencyIntentEditStatus::Invalid,
            "Project.PackageTransaction.PackageNotFound",
            "The requested package is not present in dependency intent.", {}};
    const auto modulesValue = FindProperty(
        bytes, "modules", objectStart, objectEnd);
    const auto modulesStart = modulesValue == std::string::npos
        ? std::string::npos : bytes.find('[', modulesValue);
    const auto modulesEnd = modulesStart == std::string::npos
        ? std::string::npos : Matching(bytes, modulesStart, '[', ']');
    if (modulesEnd == std::string::npos || modulesEnd >= objectEnd)
        return {DependencyIntentEditStatus::Invalid,
            "Project.PackageTransaction.ModuleSpanMissing",
            "The exact module token span could not be located.", {}};

    std::vector<std::string> modules;
    try
    {
        modules = Json::parse(bytes.substr(
            modulesStart, modulesEnd - modulesStart + 1))
            .get<std::vector<std::string>>();
    }
    catch (const std::exception& exception)
    {
        return {DependencyIntentEditStatus::Invalid,
            "Project.PackageTransaction.InvalidModules", exception.what(), {}};
    }
    const auto found = std::find(modules.begin(), modules.end(), moduleId);
    if ((operation == PackageModuleOperation::Enable && found != modules.end()) ||
        (operation == PackageModuleOperation::Disable && found == modules.end()))
        return {DependencyIntentEditStatus::NoChanges, {}, {}, bytes};
    if (operation == PackageModuleOperation::Enable)
        modules.emplace_back(moduleId);
    else
        modules.erase(found);
    if (modules.empty())
        return {DependencyIntentEditStatus::Invalid,
            "Project.PackageTransaction.EmptyDependency",
            "A dependency must retain at least one enabled module.", {}};
    SortUnique(modules);
    const auto replacement = Json(modules).dump();
    bytes.replace(modulesStart, modulesEnd - modulesStart + 1, replacement);
    return {DependencyIntentEditStatus::Changed, {}, {}, std::move(bytes)};
}

bool PackageStateCatalog::LoadInspection(
    std::string_view bytes, std::string& diagnostic)
{
    try
    {
        const auto value = Json::parse(bytes);
        if (value.at("schemaVersion").get<int>() != 1 ||
            value.at("status").get<std::string>() != "Passed" ||
            !value.at("offline").get<bool>())
            throw std::runtime_error(
                "Package inspection is not a verified offline schema v1 report.");
        std::vector<PackageCatalogModule> candidate;
        for (const auto& row : value.at("modules"))
        {
            PackageCatalogModule module;
            module.packageId = row.at("packageId").get<std::string>();
            module.version = row.at("version").get<std::string>();
            module.module = row.at("module").get<std::string>();
            module.enabled = row.at("enabled").get<bool>();
            module.runtimeReadiness =
                row.at("runtimeReadiness").get<std::string>();
            module.artifactKinds =
                row.at("artifactKinds").get<std::vector<std::string>>();
            module.apiSurfaces =
                row.at("apiSurfaces").get<std::vector<std::string>>();
            module.blocksSurfaces =
                row.at("blocksSurfaces").get<std::vector<std::string>>();
            module.capabilities =
                row.at("capabilities").get<std::vector<std::string>>();
            module.license = row.at("license").get<std::string>();
            module.trust = row.at("trust").get<std::string>();
            module.origin = row.at("origin").get<std::string>();
            module.offline = row.at("offline").get<bool>();
            if (module.module == "collectibles.lowlevel")
                throw std::runtime_error(
                    "collectibles.lowlevel is forbidden from the package catalog.");
            SortUnique(module.artifactKinds);
            SortUnique(module.apiSurfaces);
            SortUnique(module.blocksSurfaces);
            SortUnique(module.capabilities);
            candidate.push_back(std::move(module));
        }
        std::sort(candidate.begin(), candidate.end(),
            [](const auto& left, const auto& right)
            {
                return std::tie(left.packageId, left.module) <
                    std::tie(right.packageId, right.module);
            });
        modules_ = std::move(candidate);
        graphHash_ = value.at("graphHash").get<std::string>();
        generationId_ = value.at("generationId").get<std::string>();
        diagnostic.clear();
        return true;
    }
    catch (const std::exception& exception)
    {
        Invalidate();
        diagnostic = exception.what();
        return false;
    }
}

const PackageCatalogModule* PackageStateCatalog::Find(
    std::string_view packageId, std::string_view module) const noexcept
{
    const auto found = std::find_if(modules_.begin(), modules_.end(),
        [&](const auto& entry)
        {
            return entry.packageId == packageId && entry.module == module;
        });
    return found == modules_.end() ? nullptr : &*found;
}

void PackageStateCatalog::Invalidate()
{
    modules_.clear();
    graphHash_.clear();
    generationId_.clear();
}

void PackageTransactionReview::Normalize()
{
    SortUnique(moduleChanges);
    SortUnique(surfaceChanges);
    SortUnique(artifactChanges);
    SortUnique(capabilities);
    SortUnique(usageImpact);
    SortUnique(refreshPlan);
}

void ProjectReadinessSession::SetRecoveryRequired(bool value) noexcept
{ recoveryRequired_ = value; }
void ProjectReadinessSession::SetInvalid(bool value) noexcept
{ invalid_ = value; }
void ProjectReadinessSession::SetMutating(bool value) noexcept
{ mutating_ = value; }
void ProjectReadinessSession::SetDirty(bool value) noexcept
{ dirty_ = value; }
void ProjectReadinessSession::SetToolingOutOfDate(bool value) noexcept
{ toolingOutOfDate_ = value; }
void ProjectReadinessSession::SetArtifactsOutOfDate(bool value) noexcept
{ artifactsOutOfDate_ = value; }
void ProjectReadinessSession::SetConformanceFailed(bool value) noexcept
{ conformanceFailed_ = value; }

ProjectReadinessState ProjectReadinessSession::State() const noexcept
{
    if (recoveryRequired_) return ProjectReadinessState::RecoveryRequired;
    if (invalid_) return ProjectReadinessState::Invalid;
    if (mutating_) return ProjectReadinessState::Mutating;
    if (dirty_) return ProjectReadinessState::Dirty;
    if (toolingOutOfDate_) return ProjectReadinessState::ToolingOutOfDate;
    if (artifactsOutOfDate_) return ProjectReadinessState::ArtifactsOutOfDate;
    if (conformanceFailed_) return ProjectReadinessState::ConformanceFailed;
    return ProjectReadinessState::Ready;
}

struct PackageModuleWorkflow::ActiveReview final
{
    PackageWorkflowCapture base;
    PackageTransactionReview review;
    std::string candidateManifest;
    std::string candidateLock;
    std::filesystem::path stagingRoot;
    ProjectMutationLease lease;
    PackageHistoryIntent historyIntent = PackageHistoryIntent::Forward;
    bool applyAttempted = false;
};

PackageModuleWorkflow::PackageModuleWorkflow(
    ProjectReadinessSession& readiness, PackageStateCatalog& catalog)
    : readiness_(readiness), catalog_(catalog)
{
}

PackageModuleWorkflow::~PackageModuleWorkflow()
{
    ReleaseActive(true);
}

PackageWorkflowResult PackageModuleWorkflow::Prepare(
    const PackageWorkflowPrepareRequest& request)
{
    if (active_)
        return {PackageWorkflowStatus::Rejected,
            "Project.PackageTransaction.Reentrant",
            "A package review is already active.", {}};
    if (request.relevantBuffersDirty)
        return {PackageWorkflowStatus::Rejected,
            "Project.PackageTransaction.SaveAllRequired",
            "Relevant project, source, scene, input, or UI buffers must be saved first.", {}};
    if (!request.recapture || !request.resolveCandidate ||
        request.capture.projectRoot.empty() ||
        request.capture.manifestPath.empty() || request.capture.lockPath.empty())
        return {PackageWorkflowStatus::Rejected,
            "Project.PackageTransaction.InvalidRequest",
            "Capture, recapture, paths, and resolver are required.", {}};

    const auto edit = DependencyIntentEditor::Edit(request.capture.manifestBytes,
        request.packageId, request.module, request.operation);
    if (edit.status == DependencyIntentEditStatus::NoChanges)
        return {PackageWorkflowStatus::NoChanges, {}, {}, {}};
    if (edit.status != DependencyIntentEditStatus::Changed)
        return {PackageWorkflowStatus::Rejected, edit.code, edit.message, {}};

    auto [lease, acquired] = ProjectMutationLease::Acquire(
        request.capture.projectRoot, "package-module");
    if (!acquired.succeeded)
        return {PackageWorkflowStatus::Rejected, acquired.code, acquired.message, {}};
    readiness_.SetMutating(true);
    auto active = std::make_unique<ActiveReview>();
    active->base = request.capture;
    active->candidateManifest = edit.candidateBytes;
    active->historyIntent = request.historyIntent;
    active->lease = std::move(lease);
    active->review.transactionId = TransactionId();
    active->stagingRoot = active->lease.ProjectRoot() /
        ".saga/local/transactions/package" / active->review.transactionId;
    std::error_code error;
    std::filesystem::create_directories(active->stagingRoot, error);
    const auto stagedManifest = active->stagingRoot / "candidate.sagaproj";
    if (!error)
    {
        std::ofstream output(stagedManifest, std::ios::binary | std::ios::trunc);
        output.write(active->candidateManifest.data(),
            static_cast<std::streamsize>(active->candidateManifest.size()));
        output.flush();
        if (!output) error = std::make_error_code(std::errc::io_error);
    }
    if (error)
    {
        std::filesystem::remove_all(active->stagingRoot, error);
        readiness_.SetMutating(false);
        return {PackageWorkflowStatus::Rejected,
            "Project.PackageTransaction.StagingFailed", error.message(), {}};
    }

    const auto resolved = request.resolveCandidate(stagedManifest,
        active->stagingRoot);
    if (!resolved.passed)
    {
        std::filesystem::remove_all(active->stagingRoot, error);
        readiness_.SetMutating(false);
        return {PackageWorkflowStatus::Rejected,
            resolved.code.empty() ? "Project.PackageTransaction.ResolverFailed" : resolved.code,
            resolved.message, {}};
    }
    if (resolved.candidateLockBytes.empty() ||
        resolved.candidateGraphHash.empty() || resolved.inspectionBytes.empty())
    {
        std::filesystem::remove_all(active->stagingRoot, error);
        readiness_.SetMutating(false);
        return {PackageWorkflowStatus::Rejected,
            "Project.PackageTransaction.InvalidCandidate",
            "Resolver output requires a lock, graph identity, and inspection catalog.", {}};
    }
    if (!SameCapture(request.capture, request.recapture()))
    {
        std::filesystem::remove_all(active->stagingRoot, error);
        readiness_.SetMutating(false);
        return {PackageWorkflowStatus::Rejected,
            "Project.PackageTransaction.StaleReview",
            "Canonical inputs changed while the candidate was resolving.", {}};
    }
    active->candidateLock = resolved.candidateLockBytes;
    active->review.operation = request.operation;
    active->review.packageId = request.packageId;
    active->review.module = request.module;
    active->review.baseIntentHash = Sha256(request.capture.manifestBytes);
    active->review.candidateIntentHash = Sha256(active->candidateManifest);
    active->review.baseGraphHash = request.capture.graphHash;
    active->review.candidateGraphHash = resolved.candidateGraphHash;
    active->review.moduleChanges = {
        std::string(request.operation == PackageModuleOperation::Enable ? "+" : "-") +
        request.packageId + "/" + request.module};
    active->review.refreshPlan = {"Generation refresh", "Script analysis",
        "Package and Blocks catalog refresh"};

    PackageStateCatalog candidateCatalog;
    std::string catalogDiagnostic;
    if (!candidateCatalog.LoadInspection(resolved.inspectionBytes,
        catalogDiagnostic))
    {
        std::filesystem::remove_all(active->stagingRoot, error);
        readiness_.SetMutating(false);
        return {PackageWorkflowStatus::Rejected,
            "Project.PackageTransaction.InvalidCandidateCatalog",
            catalogDiagnostic, {}};
    }
    if (const auto* row = candidateCatalog.Find(request.packageId, request.module))
    {
        active->review.version = row->version;
        active->review.capabilities = row->capabilities;
        active->review.license = row->license;
        active->review.trust = row->trust;
        active->review.offline = row->offline;
        active->review.surfaceChanges = row->apiSurfaces;
        active->review.surfaceChanges.insert(active->review.surfaceChanges.end(),
            row->blocksSurfaces.begin(), row->blocksSurfaces.end());
        active->review.artifactChanges = row->artifactKinds;
    }
    if (request.operation == PackageModuleOperation::Disable)
    {
        if (!request.analyzeUsage)
        {
            std::filesystem::remove_all(active->stagingRoot, error);
            readiness_.SetMutating(false);
            return {PackageWorkflowStatus::Rejected,
                "Project.PackageTransaction.UsageAnalysisRequired",
                "Disable requires a fresh module usage analysis.", {}};
        }
        const auto usage = request.analyzeUsage(
            request.packageId, request.module, active->stagingRoot);
        const bool stale = usage.sourceInventoryHash != request.capture.sourceInventoryHash ||
            usage.generationId != request.capture.generationId ||
            usage.generationHash != request.capture.generationHash ||
            usage.graphHash != request.capture.graphHash ||
            usage.ownershipCatalogHash != request.capture.ownershipCatalogHash ||
            (!usage.projectHash.empty() &&
                usage.projectHash != request.capture.projectHash) ||
            (!usage.lockHash.empty() && usage.lockHash != request.capture.lockHash) ||
            (!usage.generationManifestHash.empty() &&
                usage.generationManifestHash !=
                    request.capture.generationManifestHash) ||
            (!usage.referenceInventoryHash.empty() &&
                usage.referenceInventoryHash !=
                    request.capture.referenceInventoryHash);
        if (!usage.passed || stale)
        {
            std::filesystem::remove_all(active->stagingRoot, error);
            readiness_.SetMutating(false);
            return {PackageWorkflowStatus::Rejected,
                stale ? "Project.PackageTransaction.StaleUsageAnalysis" : usage.code,
                stale ? "Module usage inputs changed." : usage.message, {}};
        }
        active->review.usageImpact = usage.usages;
        for (const auto& location : usage.locations)
        {
            active->review.usageImpact.push_back(
                location.sourcePath.generic_string() + ":" +
                std::to_string(location.startLine) + ":" +
                std::to_string(location.startColumn) + " " +
                location.documentationId);
        }
        if (!usage.usages.empty() || !usage.locations.empty())
        {
            active->review.blocked = true;
            const auto blockedReview = active->review;
            std::filesystem::remove_all(active->stagingRoot, error);
            readiness_.SetMutating(false);
            return {PackageWorkflowStatus::Rejected,
                "Project.PackageTransaction.ModuleInUse",
                "The module has semantic usages in saved source.", blockedReview};
        }
        if (!SameCapture(request.capture, request.recapture()))
        {
            std::filesystem::remove_all(active->stagingRoot, error);
            readiness_.SetMutating(false);
            return {PackageWorkflowStatus::Rejected,
                "Project.PackageTransaction.StaleUsageAnalysis",
                "Canonical inputs changed during module usage analysis.", {}};
        }
    }
    active->review.Normalize();
    const auto review = active->review;
    active_ = std::move(active);
    return {PackageWorkflowStatus::Prepared, {}, {}, review};
}

PackageWorkflowResult PackageModuleWorkflow::Apply(
    std::string_view transactionId,
    const std::function<PackageWorkflowCapture()>& recapture,
    const std::function<PackageRefreshResult()>& refresh)
{
    if (!active_ && transactionId == lastAppliedTransactionId_)
        return {PackageWorkflowStatus::Rejected,
            "Project.PackageTransaction.AlreadyApplied",
            "A reviewed transaction can be applied only once.", {}};
    if (!active_ || active_->review.transactionId != transactionId)
        return {PackageWorkflowStatus::Rejected,
            "Project.PackageTransaction.UnknownReview",
            "Apply accepts exactly the active reviewed transaction.", {}};
    if (active_->applyAttempted)
        return {PackageWorkflowStatus::Rejected,
            "Project.PackageTransaction.AlreadyApplied",
            "A reviewed transaction can be applied only once.", active_->review};
    active_->applyAttempted = true;
    const auto current = recapture ? recapture() : PackageWorkflowCapture{};
    if (current.manifestBytes != active_->base.manifestBytes ||
        current.lockBytes != active_->base.lockBytes)
    {
        const auto review = active_->review;
        ReleaseActive(true);
        readiness_.SetMutating(false);
        return {PackageWorkflowStatus::Rejected,
            "Project.PackageTransaction.ExternalConflict",
            "Canonical package inputs changed after review.", review};
    }
    if (current.sourceInventoryHash != active_->base.sourceInventoryHash)
    {
        const auto review = active_->review;
        ReleaseActive(true);
        readiness_.SetMutating(false);
        return {PackageWorkflowStatus::Rejected,
            "Project.PackageTransaction.StaleUsageAnalysis",
            "Saved source inventory changed after review.", review};
    }
    if (current.generationId != active_->base.generationId ||
        current.generationHash != active_->base.generationHash ||
        current.graphHash != active_->base.graphHash ||
        current.ownershipCatalogHash != active_->base.ownershipCatalogHash)
    {
        const auto review = active_->review;
        ReleaseActive(true);
        readiness_.SetMutating(false);
        return {PackageWorkflowStatus::Rejected,
            "Project.PackageTransaction.StaleReview",
            "Package generation or ownership identity changed after review.", review};
    }

    const auto manifestRelative = std::filesystem::relative(
        active_->base.manifestPath, active_->lease.ProjectRoot());
    const auto lockRelative = std::filesystem::relative(
        active_->base.lockPath, active_->lease.ProjectRoot());
    ProjectMutationTransaction transaction(active_->lease,
        active_->review.transactionId, "package-workflow-v1");
    auto mutation = transaction.Stage({
        {manifestRelative, active_->candidateManifest},
        {lockRelative, active_->candidateLock}});
    if (mutation.succeeded) mutation = transaction.MarkCommit();
    if (mutation.succeeded) mutation = transaction.Publish();
    if (!mutation.succeeded)
    {
        const auto review = active_->review;
        if (transaction.Journal().commitMarker)
        {
            readiness_.SetRecoveryRequired(true);
            ReleaseActive(false);
        }
        else
        {
            readiness_.SetMutating(false);
            ReleaseActive(true);
        }
        return {PackageWorkflowStatus::Rejected, mutation.code,
            mutation.message, review};
    }

    switch (active_->historyIntent)
    {
    case PackageHistoryIntent::Forward:
        if (historyCursor_ < history_.size()) history_.resize(historyCursor_);
        history_.push_back(active_->review);
        historyCursor_ = history_.size();
        break;
    case PackageHistoryIntent::Undo:
        if (historyCursor_ > 0) --historyCursor_;
        break;
    case PackageHistoryIntent::Redo:
        if (historyCursor_ < history_.size()) ++historyCursor_;
        break;
    }
    lastAppliedTransactionId_ = active_->review.transactionId;
    refreshPending_ = true;
    ReleaseActive(true);
    return FinishRefresh(refresh ? refresh() : PackageRefreshResult{},
        PackageWorkflowStatus::Applied);
}

PackageWorkflowResult PackageModuleWorkflow::Cancel()
{
    if (!active_)
        return {PackageWorkflowStatus::Rejected,
            "Project.PackageTransaction.NoActiveReview",
            "There is no review to cancel.", {}};
    ReleaseActive(true);
    readiness_.SetMutating(false);
    return {PackageWorkflowStatus::Cancelled, {}, {}, {}};
}

PackageWorkflowResult PackageModuleWorkflow::RetryRefresh(
    const std::function<PackageRefreshResult()>& refresh)
{
    if (!refreshPending_)
        return {PackageWorkflowStatus::Rejected,
            "Project.PackageTransaction.NoRefreshPending",
            "No committed package input requires refresh.", {}};
    return FinishRefresh(refresh ? refresh() : PackageRefreshResult{},
        PackageWorkflowStatus::Applied);
}

PackageWorkflowResult PackageModuleWorkflow::FinishRefresh(
    const PackageRefreshResult& refresh, PackageWorkflowStatus success)
{
    readiness_.SetMutating(false);
    if (!refresh.passed)
    {
        refreshPending_ = true;
        readiness_.SetToolingOutOfDate(true);
        catalog_.Invalidate();
        return {PackageWorkflowStatus::ToolingOutOfDate,
            refresh.code.empty() ? "Project.PackageTransaction.RefreshFailed" : refresh.code,
            refresh.message, {}};
    }
    std::string diagnostic;
    if (!catalog_.LoadInspection(refresh.inspectionBytes, diagnostic))
    {
        refreshPending_ = true;
        readiness_.SetToolingOutOfDate(true);
        return {PackageWorkflowStatus::ToolingOutOfDate,
            "Project.PackageTransaction.RefreshCatalogInvalid", diagnostic, {}};
    }
    refreshPending_ = false;
    readiness_.SetToolingOutOfDate(false);
    return {success, {}, {}, {}};
}

void PackageModuleWorkflow::ReleaseActive(bool removeStaging)
{
    if (!active_) return;
    if (removeStaging)
    {
        std::error_code error;
        std::filesystem::remove_all(active_->stagingRoot, error);
    }
    active_.reset();
}

bool PackageModuleWorkflow::Active() const noexcept { return active_ != nullptr; }
const std::vector<PackageTransactionReview>& PackageModuleWorkflow::History() const noexcept
{ return history_; }

std::optional<PackageTransactionReview> PackageModuleWorkflow::UndoIntent() const
{
    if (historyCursor_ == 0) return std::nullopt;
    auto review = history_[historyCursor_ - 1];
    review.transactionId.clear();
    review.operation = review.operation == PackageModuleOperation::Enable
        ? PackageModuleOperation::Disable : PackageModuleOperation::Enable;
    return review;
}

std::optional<PackageTransactionReview> PackageModuleWorkflow::RedoIntent() const
{
    if (historyCursor_ >= history_.size()) return std::nullopt;
    auto review = history_[historyCursor_];
    review.transactionId.clear();
    return review;
}
}
