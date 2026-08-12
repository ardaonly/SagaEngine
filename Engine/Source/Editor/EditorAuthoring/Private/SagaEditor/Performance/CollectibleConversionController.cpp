// SPDX-License-Identifier: MPL-2.0

#include "SagaEditor/Performance/CollectibleConversionController.h"
#include "SagaEditor/Performance/CollectibleConversionHistory.h"

#include "SagaEditor/Project/ProjectMutation.h"

#include <nlohmann/json.hpp>
#include <openssl/sha.h>

#include <array>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace SagaEditor
{
namespace
{
namespace fs = std::filesystem;
using Json = nlohmann::json;
constexpr std::string_view BeginMarker = "// SAGA-COLLECTIBLE-SURFACE-BEGIN";
constexpr std::string_view EndMarker = "// SAGA-COLLECTIBLE-SURFACE-END";
constexpr std::size_t MaxConversionFileBytes = 8 * 1024 * 1024;

std::string Read(const fs::path& path)
{
    std::ifstream input(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

std::string Hash(std::string_view bytes)
{
    std::array<unsigned char, SHA256_DIGEST_LENGTH> digest{};
    SHA256(reinterpret_cast<const unsigned char*>(bytes.data()), bytes.size(), digest.data());
    std::ostringstream output;
    output << "sha256:";
    for (const auto byte : digest)
        output << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(byte);
    return output.str();
}

std::optional<std::string_view> Region(std::string_view source)
{
    const auto begin = source.find(BeginMarker);
    const auto end = source.find(EndMarker);
    if (begin == std::string_view::npos || end == std::string_view::npos || end <= begin ||
        source.find(BeginMarker, begin + BeginMarker.size()) != std::string_view::npos ||
        source.find(EndMarker, end + EndMarker.size()) != std::string_view::npos) return std::nullopt;
    return source.substr(begin, end + EndMarker.size() - begin);
}

bool OnlySupportedRegionChanged(std::string_view source, std::string_view candidate)
{
    const auto sourceBegin = source.find(BeginMarker);
    const auto sourceEnd = source.find(EndMarker);
    const auto candidateBegin = candidate.find(BeginMarker);
    const auto candidateEnd = candidate.find(EndMarker);
    if (sourceBegin == std::string_view::npos || sourceEnd == std::string_view::npos ||
        candidateBegin == std::string_view::npos || candidateEnd == std::string_view::npos)
        return false;
    return source.substr(0, sourceBegin) == candidate.substr(0, candidateBegin) &&
        source.substr(sourceEnd + EndMarker.size()) ==
            candidate.substr(candidateEnd + EndMarker.size());
}

bool Within(const fs::path& root, const fs::path& path)
{
    const auto relative = path.lexically_relative(root);
    return !relative.empty() && !relative.is_absolute() && *relative.begin() != "..";
}

bool HasExclusiveModule(const Json& root, std::string_view collection,
    std::string_view required, std::string_view forbidden,
    std::string_view requiredVersion = {})
{
    try
    {
        for (const auto& dependency : root.at(collection))
        {
            if (dependency.value("packageId", "") != "com.saga.gameplay.challenge") continue;
            if (!requiredVersion.empty() &&
                dependency.value("version", "") != requiredVersion) return false;
            bool hasRequired = false;
            bool hasForbidden = false;
            for (const auto& module : dependency.at("modules"))
            {
                hasRequired = hasRequired || module == required;
                hasForbidden = hasForbidden || module == forbidden;
            }
            return hasRequired && !hasForbidden;
        }
    }
    catch (const std::exception&) {}
    return false;
}

bool ValidPackageCandidates(std::string_view projectBytes, std::string_view lockBytes,
    std::string_view required, std::string_view forbidden, std::string_view requiredVersion)
{
    try
    {
        const auto project = Json::parse(projectBytes);
        const auto lock = Json::parse(lockBytes);
        return lock.value("schemaVersion", 0) == 1 &&
            !lock.value("graphHash", "").empty() &&
            !lock.value("projectId", "").empty() &&
            HasExclusiveModule(project, "dependencies", required, forbidden, requiredVersion) &&
            HasExclusiveModule(lock, "packages", required, forbidden, requiredVersion);
    }
    catch (const std::exception&) {}
    return false;
}

std::pair<std::string, std::string> PackageIdentities(
    std::string_view projectBytes, std::string_view lockBytes)
{
    try
    {
        const auto project = Json::parse(projectBytes);
        const auto lock = Json::parse(lockBytes);
        const auto projectId = project.value("projectId", "");
        const auto lockProjectId = lock.value("projectId", "");
        if (projectId.empty() || projectId != lockProjectId) return {};
        return {projectId, lock.value("graphHash", "")};
    }
    catch (const std::exception&) {}
    return {};
}

CollectibleConversionReview Fail(CollectibleConversionController& controller,
    CollectibleConversionState state, std::string code, std::string message)
{
    (void)controller;
    return {state, std::move(code), std::move(message), std::nullopt};
}
}

struct CollectibleConversionController::Active
{
    CollectibleConversionCapture capture;
    CollectibleConversionPlan plan;
    std::string candidateProject;
    std::string candidateLock;
    std::string candidateSource;
};

CollectibleConversionController::CollectibleConversionController() = default;
CollectibleConversionController::~CollectibleConversionController() = default;

CollectibleConversionReview CollectibleConversionController::Prepare(
    const CollectibleConversionCapture& capture, std::string candidateProject,
    std::string candidateLock, std::string candidateSource,
    std::string targetPackageVersion)
{
    active_.reset();
    state_ = CollectibleConversionState::Rejected;
    std::error_code error;
    const auto root = fs::canonical(capture.projectRoot, error);
    const auto canonicalProject = fs::canonical(capture.projectFile, error);
    const auto canonicalLock = fs::canonical(capture.lockFile, error);
    const auto canonicalSource = fs::canonical(capture.sourceFile, error);
    const auto canonicalProvenance = fs::canonical(capture.provenanceFile, error);
    if (error || !fs::is_regular_file(canonicalProject) ||
        !fs::is_regular_file(canonicalLock) || !fs::is_regular_file(canonicalSource) ||
        !fs::is_regular_file(canonicalProvenance) || !Within(root, canonicalProject) ||
        !Within(root, canonicalLock) || !Within(root, canonicalSource) ||
        !Within(root, canonicalProvenance))
        return Fail(*this, state_, "Conversion.CaptureInvalid",
            "Conversion requires canonical project, lock, source, and provenance files.");
    const auto source = Read(capture.sourceFile);
    const auto project = Read(capture.projectFile);
    const auto lock = Read(capture.lockFile);
    const auto provenance = Read(capture.provenanceFile);
    if (source.size() > MaxConversionFileBytes || project.size() > MaxConversionFileBytes ||
        lock.size() > MaxConversionFileBytes || provenance.size() > MaxConversionFileBytes ||
        candidateSource.size() > MaxConversionFileBytes ||
        candidateProject.size() > MaxConversionFileBytes ||
        candidateLock.size() > MaxConversionFileBytes)
        return Fail(*this, state_, "Conversion.InputTooLarge",
            "Conversion inputs must not exceed 8 MiB per file.");
    const auto oldRegion = Region(source);
    const auto newRegion = Region(candidateSource);
    if (!oldRegion || !newRegion || !OnlySupportedRegionChanged(source, candidateSource) ||
        newRegion->find("CollectiblesLowLevelBatch") == std::string_view::npos ||
        newRegion->find("CollectiblesHighLevel") != std::string_view::npos)
        return Fail(*this, state_, "Conversion.UnsupportedSource",
            "The source must contain one supported collectible region and a low-level candidate.");
    if (!ValidPackageCandidates(project, lock, "collectibles.highlevel",
            "collectibles.lowlevel", targetPackageVersion) ||
        !ValidPackageCandidates(candidateProject, candidateLock, "collectibles.lowlevel",
            "collectibles.highlevel", targetPackageVersion))
        return Fail(*this, state_, "Conversion.PackageOwnershipInvalid",
            "High-level and low-level module ownership must be mutually exclusive.");
    const auto [projectId, graphHash] = PackageIdentities(project, lock);
    const auto [candidateProjectId, candidateGraphHash] =
        PackageIdentities(candidateProject, candidateLock);
    if (projectId.empty() || candidateProjectId != projectId ||
        graphHash != capture.packageGraphHash || candidateGraphHash.empty())
        return Fail(*this, state_, "Conversion.PackageIdentityMismatch",
            "Project and verified lock identities must remain coherent across conversion.");
    if (capture.projectGeneration.empty() || capture.packageGraphHash.empty() ||
        capture.sourceRevision.empty() || candidateLock.empty() || targetPackageVersion.empty())
        return Fail(*this, state_, "Conversion.IdentityMissing",
            "Conversion identities and verified lock candidate are required.");

    CollectibleConversionPlan plan;
    plan.sourceHash = Hash(source);
    plan.candidateSourceHash = Hash(candidateSource);
    plan.projectHash = Hash(project);
    plan.candidateProjectHash = Hash(candidateProject);
    plan.lockHash = Hash(lock);
    plan.candidateLockHash = Hash(candidateLock);
    plan.provenanceHash = Hash(provenance);
    plan.reversePatch = source;
    plan.projectGeneration = capture.projectGeneration;
    plan.packageGraphHash = capture.packageGraphHash;
    plan.sourceRevision = capture.sourceRevision;
    plan.targetPackageId = "com.saga.gameplay.challenge";
    plan.targetPackageVersion = std::move(targetPackageVersion);
    plan.supportedRegionChecksums = {Hash(*oldRegion), Hash(*newRegion)};
    plan.deterministicDiff = "collectibles.highlevel -> collectibles.lowlevel\n" +
        plan.sourceHash + " -> " + plan.candidateSourceHash + "\n";
    plan.operationId = Hash("Apply\n" + plan.deterministicDiff + plan.projectHash + "\n" +
        plan.candidateProjectHash + "\n" + plan.packageGraphHash + "\n" +
        plan.projectGeneration + "\n");
    const Json provenanceCandidate{{"schemaVersion", 1}, {"toolVersion", "0.0.11"},
        {"state", "Applied"}, {"sourceHash", plan.sourceHash},
        {"candidateSourceHash", plan.candidateSourceHash},
        {"projectHash", plan.projectHash}, {"candidateProjectHash", plan.candidateProjectHash},
        {"lockHash", plan.lockHash}, {"candidateLockHash", plan.candidateLockHash},
        {"packageId", plan.targetPackageId}, {"packageVersion", plan.targetPackageVersion},
        {"projectGeneration", plan.projectGeneration}, {"packageGraphHash", plan.packageGraphHash},
        {"sourceRevision", plan.sourceRevision},
        {"supportedRegionChecksums", plan.supportedRegionChecksums},
        {"reversePatch", plan.reversePatch}, {"projectBytes", project},
        {"lockBytes", lock}};
    plan.candidateProvenance = provenanceCandidate.dump(2) + "\n";
    active_ = std::make_unique<Active>(Active{capture, plan, std::move(candidateProject),
        std::move(candidateLock), std::move(candidateSource)});
    state_ = CollectibleConversionState::AwaitingReview;
    return {state_, {}, {}, plan};
}

CollectibleConversionReview CollectibleConversionController::Apply()
{
    if (!active_ || state_ != CollectibleConversionState::AwaitingReview)
        return Fail(*this, state_, "Conversion.NoReview", "No conversion is awaiting review.");
    if (Hash(Read(active_->capture.projectFile)) != active_->plan.projectHash ||
        Hash(Read(active_->capture.lockFile)) != active_->plan.lockHash ||
        Hash(Read(active_->capture.sourceFile)) != active_->plan.sourceHash ||
        Hash(Read(active_->capture.provenanceFile)) != active_->plan.provenanceHash)
    {
        active_.reset();
        state_ = CollectibleConversionState::Rejected;
        return Fail(*this, state_, "Conversion.StaleReview",
            "Project, package, source, or provenance changed after review.");
    }
    auto [lease, acquired] = ProjectMutationLease::Acquire(
        active_->capture.projectRoot, "collectible-conversion");
    if (!acquired.succeeded)
    {
        state_ = CollectibleConversionState::Rejected;
        return Fail(*this, CollectibleConversionState::Rejected, acquired.code, acquired.message);
    }
    ProjectMutationTransaction transaction(lease, active_->plan.operationId, "0.0.11");
    const auto historyPath = CollectibleConversionHistory::Path(
        active_->capture.projectRoot);
    const auto history = CollectibleConversionHistory::Append(historyPath,
        {active_->plan.operationId, CollectibleConversionHistoryState::Applied,
            Hash(active_->plan.deterministicDiff), active_->plan.sourceHash,
            active_->plan.candidateSourceHash, active_->plan.candidateProjectHash,
            active_->plan.packageGraphHash, active_->plan.projectGeneration,
            "SagaEditor.CollectibleConversionController/0.0.11",
            Hash(active_->plan.candidateProvenance)}, active_->plan.candidateProvenance);
    if (!history.succeeded)
        return Fail(*this, CollectibleConversionState::Rejected,
            history.code, history.message);
    const std::vector<ProjectMutationTransaction::CandidateFile> files{
        {fs::relative(active_->capture.projectFile, active_->capture.projectRoot), active_->candidateProject},
        {fs::relative(active_->capture.lockFile, active_->capture.projectRoot), active_->candidateLock},
        {fs::relative(active_->capture.sourceFile, active_->capture.projectRoot), active_->candidateSource},
        {fs::relative(active_->capture.provenanceFile, active_->capture.projectRoot),
            history.candidateBytes}};
    if (auto result = transaction.Stage(files); !result.succeeded)
    {
        state_ = CollectibleConversionState::Rejected;
        return Fail(*this, CollectibleConversionState::Rejected, result.code, result.message);
    }
    if (auto result = transaction.MarkCommit(); !result.succeeded)
    {
        state_ = CollectibleConversionState::Rejected;
        return Fail(*this, CollectibleConversionState::Rejected, result.code, result.message);
    }
    if (auto result = transaction.Publish(); !result.succeeded)
    {
        state_ = CollectibleConversionState::Rejected;
        return Fail(*this, CollectibleConversionState::Rejected, result.code, result.message);
    }
    const auto plan = active_->plan;
    active_.reset();
    state_ = CollectibleConversionState::Applied;
    return {state_, {}, {}, plan};
}

CollectibleConversionReview CollectibleConversionController::Reverse(
    const CollectibleConversionCapture& capture)
{
    Json provenance;
    try { provenance = Json::parse(Read(capture.provenanceFile)); }
    catch (const std::exception& exception)
    {
        state_ = CollectibleConversionState::DetachedRequiresReview;
        return Fail(*this, state_, "Conversion.ProvenanceInvalid", exception.what());
    }
    const auto source = Read(capture.sourceFile);
    const auto project = Read(capture.projectFile);
    const auto lock = Read(capture.lockFile);
    const auto historyGeneration = provenance.value("projectGeneration",
        capture.projectGeneration);
    const auto historyGraph = provenance.value("packageGraphHash",
        capture.packageGraphHash);
    if (provenance.value("schemaVersion", 0) != 1 ||
        provenance.value("state", "") != "Applied" ||
        Hash(source) != provenance.value("candidateSourceHash", "") ||
        Hash(project) != provenance.value("candidateProjectHash", "") ||
        Hash(lock) != provenance.value("candidateLockHash", ""))
    {
        state_ = CollectibleConversionState::DetachedRequiresReview;
        const auto historyPath = CollectibleConversionHistory::Path(capture.projectRoot);
        const auto operationId = Hash("DetachedRequiresReview\n" +
            Read(capture.provenanceFile) + source + project + lock);
        const auto history = CollectibleConversionHistory::Append(historyPath,
            {operationId, CollectibleConversionHistoryState::DetachedRequiresReview,
                Hash(Read(capture.provenanceFile)), Hash(source), Hash(source), Hash(project),
                historyGraph, historyGeneration,
                "SagaEditor.CollectibleConversionController/0.0.11",
                Hash(Read(capture.provenanceFile))});
        if (!history.succeeded) return Fail(*this, state_, history.code, history.message);
        auto [lease, acquired] = ProjectMutationLease::Acquire(
            capture.projectRoot, "collectible-conversion-detached-history");
        if (!acquired.succeeded)
            return Fail(*this, state_, acquired.code, acquired.message);
        ProjectMutationTransaction transaction(lease, operationId, "0.0.11");
        const std::vector<ProjectMutationTransaction::CandidateFile> files{
            {historyPath.lexically_relative(capture.projectRoot), history.candidateBytes}};
        if (auto result = transaction.Stage(files); !result.succeeded)
            return Fail(*this, state_, result.code, result.message);
        if (auto result = transaction.MarkCommit(); !result.succeeded)
            return Fail(*this, state_, result.code, result.message);
        if (auto result = transaction.Publish(); !result.succeeded)
            return Fail(*this, state_, result.code, result.message);
        return Fail(*this, state_, "Conversion.Detached",
            "Current source or package identities diverged from conversion provenance.");
    }
    const auto reverseSource = provenance.value("reversePatch", "");
    if (!Region(reverseSource))
    {
        state_ = CollectibleConversionState::DetachedRequiresReview;
        return Fail(*this, CollectibleConversionState::DetachedRequiresReview,
            "Conversion.ReversePatchInvalid", "The exact reverse patch is invalid.");
    }
    const auto reverseProject = provenance.value("projectBytes", "");
    const auto reverseLock = provenance.value("lockBytes", "");
    if (reverseProject.empty() || reverseLock.empty())
    {
        state_ = CollectibleConversionState::DetachedRequiresReview;
        return Fail(*this, CollectibleConversionState::DetachedRequiresReview,
            "Conversion.ReversePackageMissing", "Reverse package bytes are unavailable.");
    }
    auto [lease, acquired] = ProjectMutationLease::Acquire(
        capture.projectRoot, "collectible-conversion-reverse");
    if (!acquired.succeeded)
        return Fail(*this, CollectibleConversionState::DetachedRequiresReview,
            acquired.code, acquired.message);
    const auto operationId = Hash("Reverse\n" + Read(capture.provenanceFile) +
        reverseSource + reverseProject + reverseLock);
    ProjectMutationTransaction transaction(lease, operationId, "0.0.11");
    const std::string resetProvenance = "{\n  \"schemaVersion\": 1,\n  \"state\": \"HighLevel\"\n}\n";
    const auto historyPath = CollectibleConversionHistory::Path(capture.projectRoot);
    const auto history = CollectibleConversionHistory::Append(historyPath,
        {operationId, CollectibleConversionHistoryState::Reversed,
            Hash(Read(capture.provenanceFile)), Hash(source), Hash(reverseSource),
            Hash(reverseProject), historyGraph, historyGeneration,
            "SagaEditor.CollectibleConversionController/0.0.11",
            Hash(resetProvenance)}, resetProvenance);
    if (!history.succeeded)
        return Fail(*this, CollectibleConversionState::DetachedRequiresReview,
            history.code, history.message);
    const std::vector<ProjectMutationTransaction::CandidateFile> files{
        {fs::relative(capture.projectFile, capture.projectRoot), reverseProject},
        {fs::relative(capture.lockFile, capture.projectRoot), reverseLock},
        {fs::relative(capture.sourceFile, capture.projectRoot), reverseSource},
        {fs::relative(capture.provenanceFile, capture.projectRoot), history.candidateBytes}};
    if (auto result = transaction.Stage(files); !result.succeeded)
        return Fail(*this, CollectibleConversionState::DetachedRequiresReview,
            result.code, result.message);
    if (auto result = transaction.MarkCommit(); !result.succeeded)
        return Fail(*this, CollectibleConversionState::DetachedRequiresReview,
            result.code, result.message);
    if (auto result = transaction.Publish(); !result.succeeded)
        return Fail(*this, CollectibleConversionState::DetachedRequiresReview,
            result.code, result.message);
    state_ = CollectibleConversionState::Reversed;
    return {state_, {}, {}, std::nullopt};
}

CollectibleConversionReview CollectibleConversionController::Cancel()
{
    active_.reset();
    state_ = CollectibleConversionState::Ready;
    return {state_, {}, {}, std::nullopt};
}
}
