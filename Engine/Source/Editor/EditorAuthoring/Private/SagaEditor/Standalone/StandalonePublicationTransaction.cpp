// SPDX-License-Identifier: MPL-2.0

#include "SagaEditor/Standalone/StandalonePublicationTransaction.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <fstream>

namespace SagaEditor
{
namespace
{
namespace fs = std::filesystem;
using Json = nlohmann::ordered_json;
constexpr std::string_view Owner = "SagaEditor.StandalonePublicationTransaction/v2";
constexpr std::string_view JournalName = "publish-journal.json";
constexpr std::string_view NamespaceName = ".saga-standalone-publication";

DocumentDiagnostic Diagnostic(std::string code, std::string message, fs::path path = {})
{ return {std::move(code), std::move(message), std::move(path), 0, 0}; }

std::string HashFile(const fs::path& path, std::error_code& error)
{
    const auto bytes = ReadDocumentBytes(path, error);
    return error ? std::string{} : HashDocumentBytes(bytes);
}

bool ReadJson(const fs::path& path, Json& value)
{
    std::ifstream input(path, std::ios::binary);
    if (!input) return false;
    try { value = Json::parse(input); return value.is_object(); }
    catch (const Json::exception&) { return false; }
}

bool IsHash(std::string_view value)
{
    if (value.starts_with("sha256:")) value.remove_prefix(7);
    return value.size() == 64 && std::all_of(value.begin(), value.end(), [](char value)
    { return (value >= '0' && value <= '9') || (value >= 'a' && value <= 'f'); });
}

bool LexicallyContained(const fs::path& root, const fs::path& path)
{
    const auto relative = path.lexically_normal().lexically_relative(root.lexically_normal());
    return !relative.empty() && !relative.is_absolute() && *relative.begin() != "..";
}

std::string DestinationIdentity(const fs::path& output)
{
    auto hash = HashDocumentBytes(fs::absolute(output).lexically_normal().generic_string());
    if (hash.starts_with("sha256:")) hash.erase(0, 7);
    return hash;
}

fs::path TransactionDirectory(const fs::path& output, std::string_view transactionId)
{
    if (transactionId.starts_with("sha256:")) transactionId.remove_prefix(7);
    return fs::absolute(output.parent_path()).lexically_normal() / NamespaceName /
        DestinationIdentity(output) / std::string(transactionId);
}

bool HasSymlink(const fs::path& root, const fs::path& path)
{
    std::error_code error;
    auto cursor = root;
    const auto relative = path.lexically_normal().lexically_relative(root.lexically_normal());
    for (const auto& part : relative)
    {
        cursor /= part;
        const bool symlink = fs::is_symlink(cursor, error);
        if (symlink) return true;
        error.clear();
    }
    return false;
}

using Inventory = std::map<std::string, std::string>;

bool InventoryTree(const fs::path& root, Inventory& inventory)
{
    std::error_code error;
    if (!fs::is_directory(root, error) || error || fs::is_symlink(root, error)) return false;
    for (const auto& entry : fs::recursive_directory_iterator(root, error))
    {
        if (error || entry.is_symlink()) return false;
        if (entry.is_directory()) continue;
        if (!entry.is_regular_file()) return false;
        const auto relative = fs::relative(entry.path(), root, error).generic_string();
        const auto hash = error ? std::string{} : HashFile(entry.path(), error);
        if (error || relative.empty() || !IsHash(hash)) return false;
        inventory.emplace(relative, hash);
    }
    return !inventory.empty();
}

Json InventoryJson(const Inventory& inventory)
{
    Json result = Json::object();
    for (const auto& [path, hash] : inventory) result[path] = hash;
    return result;
}

bool ParseInventory(const Json& value, Inventory& inventory)
{
    if (!value.is_object()) return false;
    for (const auto& [path, hashValue] : value.items())
    {
        const fs::path parsed(path);
        if (parsed.is_absolute() || path.empty() || path.find('\\') != std::string::npos ||
            parsed.lexically_normal().generic_string() != path || *parsed.begin() == ".." ||
            !hashValue.is_string() || !IsHash(hashValue.get<std::string>())) return false;
        if (!inventory.emplace(path, hashValue.get<std::string>()).second) return false;
    }
    return !inventory.empty();
}

bool VerifyTree(const fs::path& root, const Inventory& expected)
{
    Inventory actual;
    return InventoryTree(root, actual) && actual == expected;
}

bool VerifyManifest(const fs::path& root, std::string_view snapshotId,
                    std::string_view manifestHash)
{
    Json manifest;
    if (!ReadJson(root / "StandaloneManifest.saga.json", manifest) ||
        manifest.value("contentIdentityHash", "") != manifestHash) return false;
    const auto identity = manifest.find("contentIdentity");
    if (identity == manifest.end() || !identity->is_object() ||
        identity->value("snapshotId", "") != snapshotId) return false;
    const auto artifacts = identity->find("artifacts");
    if (artifacts == identity->end() || !artifacts->is_object() ||
        artifacts->value("snapshot", "") != snapshotId) return false;
    std::error_code error;
    return artifacts->value("executable", "") == HashFile(root / "bin/SagaRuntime", error) &&
        !error;
}

bool VerifyExistingManifest(const fs::path& root)
{
    Json manifest;
    if (!ReadJson(root / "StandaloneManifest.saga.json", manifest)) return false;
    const auto identity = manifest.find("contentIdentity");
    if (identity == manifest.end() || !identity->is_object()) return false;
    return VerifyManifest(root, identity->value("snapshotId", ""),
        manifest.value("contentIdentityHash", ""));
}

bool WriteJournal(const fs::path& path, Json& journal, std::string_view phase,
                  std::error_code& error)
{
    journal["phase"] = phase;
    return AtomicWriteDocument(path, journal.dump(2) + "\n",
        journal.value("snapshotId", "publication"), error);
}

struct OwnedJournal
{
    fs::path journalPath;
    fs::path staging;
    fs::path candidate;
    fs::path destination;
    fs::path backup;
    std::string snapshotId;
    std::string manifestHash;
    std::string phase;
    Inventory candidateInventory;
    Inventory previousInventory;
    bool hadPrevious = false;
    bool previousVerified = false;
    Json json;
};

bool ParseOwnedJournal(const fs::path& output, const fs::path& journalPath,
                       OwnedJournal& owned)
{
    try
    {
    Json journal;
    if (!ReadJson(journalPath, journal) || journal.size() != 13 ||
        journal.value("schemaVersion", 0) != 2 ||
        journal.value("owner", "") != Owner ||
        journal.value("destinationIdentity", "") != DestinationIdentity(output) ||
        journal.value("destinationName", "") != output.filename().generic_string() ||
        journal.value("candidate", "") != "candidate" ||
        journal.value("backup", "") != "previous") return false;
    const auto parent = fs::absolute(output.parent_path()).lexically_normal();
    owned.journalPath = journalPath;
    owned.staging = fs::absolute(journalPath.parent_path()).lexically_normal();
    owned.candidate = owned.staging / "candidate";
    owned.destination = parent / output.filename();
    owned.backup = owned.staging / "previous";
    owned.snapshotId = journal.value("snapshotId", "");
    owned.manifestHash = journal.value("manifestHash", "");
    owned.phase = journal.value("phase", "");
    owned.hadPrevious = journal.value("hadPrevious", false);
    owned.previousVerified = journal.value("previousVerified", false);
    owned.json = std::move(journal);
    if (owned.staging.parent_path().parent_path() != parent / NamespaceName ||
        owned.staging.parent_path().filename() != DestinationIdentity(output) ||
        owned.staging.filename().empty() ||
        !LexicallyContained(owned.staging, owned.candidate) ||
        !LexicallyContained(owned.staging, owned.backup) ||
        owned.destination == owned.staging || owned.destination == owned.candidate ||
        owned.destination == owned.backup ||
        HasSymlink(parent, owned.staging) || HasSymlink(owned.staging, owned.candidate) ||
        HasSymlink(owned.staging, owned.backup) || !IsHash(owned.snapshotId) ||
        !IsHash(owned.manifestHash) ||
        !ParseInventory(owned.json["candidateInventory"], owned.candidateInventory)) return false;
    if (!owned.json["previousInventory"].is_object() ||
        (!owned.previousVerified && !owned.json["previousInventory"].empty())) return false;
    if (owned.previousVerified &&
        !ParseInventory(owned.json["previousInventory"], owned.previousInventory)) return false;
    return owned.phase == "Prepared" || owned.phase == "PreviousMoved" ||
        owned.phase == "CandidatePublished" || owned.phase == "Completed";
    }
    catch (const Json::exception&) { return false; }
}

bool Restore(OwnedJournal& owned, StandalonePublicationRecoveryResult& result)
{
    std::error_code error;
    if (!owned.hadPrevious || !owned.previousVerified ||
        !VerifyExistingManifest(owned.backup) ||
        !VerifyTree(owned.backup, owned.previousInventory))
    {
        result.state = StandalonePublicationRecoveryState::FailedClosed;
        result.diagnostics.push_back(Diagnostic("Build.Publish.RecoveryBackupInvalid",
            "Publication recovery could not verify the previous output; transaction was retained.",
            owned.backup));
        return false;
    }
    if (fs::exists(owned.destination, error))
    {
        if (VerifyTree(owned.destination, owned.candidateInventory))
            fs::rename(owned.destination, owned.candidate, error);
        else
        {
            result.state = StandalonePublicationRecoveryState::FailedClosed;
            result.diagnostics.push_back(Diagnostic("Build.Publish.RecoveryDestinationAmbiguous",
                "Publication recovery found an unverified destination; transaction was retained.",
                owned.destination));
            return false;
        }
    }
    if (!error) fs::rename(owned.backup, owned.destination, error);
    if (error || !VerifyTree(owned.destination, owned.previousInventory))
    {
        result.state = StandalonePublicationRecoveryState::FailedClosed;
        result.diagnostics.push_back(Diagnostic("Build.Publish.RestoreFailed",
            "Restoring the verified previous output failed; journal and backup were retained.",
            owned.backup));
        return false;
    }
    result.state = StandalonePublicationRecoveryState::PreviousRestored;
    result.recovered = true;
    result.preservedPreviousOutput = true;
    return true;
}

StandalonePublicationRecoveryResult RecoverJournal(
    const fs::path& output, const fs::path& journalPath)
{
    StandalonePublicationRecoveryResult result;
    OwnedJournal owned;
    if (!ParseOwnedJournal(output, journalPath, owned))
    {
        result.state = StandalonePublicationRecoveryState::FailedClosed;
        result.diagnostics.push_back(Diagnostic("Build.Publish.JournalUntrusted",
            "Publication journal ownership, containment, or hashes could not be verified.",
            journalPath));
        return result;
    }
    std::error_code error;
    if (owned.phase == "Completed")
    {
        if (!VerifyTree(owned.destination, owned.candidateInventory))
        {
            result.state = StandalonePublicationRecoveryState::FailedClosed;
            result.diagnostics.push_back(Diagnostic("Build.Publish.CompletedOutputInvalid",
                "Completed publication no longer matches its recorded inventory.", owned.destination));
            return result;
        }
        fs::remove_all(owned.staging, error);
        if (error)
        {
            result.state = StandalonePublicationRecoveryState::FailedClosed;
            result.diagnostics.push_back(Diagnostic("Build.Publish.RecoveryCleanupFailed",
                "Completed publication cleanup failed; evidence was retained.", owned.staging));
            return result;
        }
        result.state = StandalonePublicationRecoveryState::Completed;
        result.recovered = true;
        return result;
    }

    if (owned.phase == "Prepared" && owned.hadPrevious)
    {
        const bool candidateValid = VerifyTree(owned.candidate, owned.candidateInventory);
        const bool destinationPrevious = VerifyExistingManifest(owned.destination) &&
            VerifyTree(owned.destination, owned.previousInventory);
        const bool backupPrevious = VerifyExistingManifest(owned.backup) &&
            VerifyTree(owned.backup, owned.previousInventory);
        const bool destinationExists = fs::exists(owned.destination, error);
        error.clear();
        const bool backupExists = fs::exists(owned.backup, error);
        error.clear();
        if (!owned.previousVerified || !candidateValid ||
            (!destinationPrevious && !backupPrevious) ||
            (destinationExists && backupExists))
        {
            result.state = StandalonePublicationRecoveryState::FailedClosed;
            result.diagnostics.push_back(Diagnostic("Build.Publish.PreparedInvalid",
                "Prepared publication inputs changed; transaction was retained.", owned.staging));
            return result;
        }
        if (destinationPrevious && !backupExists)
            fs::rename(owned.destination, owned.backup, error);
        else if (destinationExists || !backupPrevious)
            error = std::make_error_code(std::errc::state_not_recoverable);
        if (!error) WriteJournal(owned.journalPath, owned.json, "PreviousMoved", error);
        owned.phase = "PreviousMoved";
    }
    if (owned.phase == "Prepared" && !owned.hadPrevious)
    {
        if (!VerifyTree(owned.candidate, owned.candidateInventory))
        {
            result.state = StandalonePublicationRecoveryState::FailedClosed;
            result.diagnostics.push_back(Diagnostic("Build.Publish.CandidateInvalid",
                "Prepared candidate changed; transaction was retained.", owned.candidate));
            return result;
        }
        owned.phase = "PreviousMoved";
        WriteJournal(owned.journalPath, owned.json, owned.phase, error);
    }
    if (error)
    {
        result.state = StandalonePublicationRecoveryState::FailedClosed;
        result.diagnostics.push_back(Diagnostic("Build.Publish.RecoveryTransitionFailed",
            error.message(), owned.staging));
        return result;
    }
    if (owned.phase == "PreviousMoved")
    {
        const bool candidateValid = VerifyTree(owned.candidate, owned.candidateInventory);
        const bool destinationValid = VerifyTree(
            owned.destination, owned.candidateInventory);
        const bool candidateExists = fs::exists(owned.candidate, error);
        error.clear();
        const bool destinationExists = fs::exists(owned.destination, error);
        error.clear();
        if (candidateValid && !destinationExists)
        {
            fs::rename(owned.candidate, owned.destination, error);
        }
        else if (!candidateExists && destinationValid)
        {
            // The candidate rename completed before its journal update.
        }
        else
        {
            if (owned.hadPrevious && Restore(owned, result))
            {
                fs::remove_all(owned.staging, error);
                if (error)
                {
                    result.state = StandalonePublicationRecoveryState::FailedClosed;
                    result.diagnostics.push_back(Diagnostic(
                        "Build.Publish.RecoveryCleanupFailed",
                        "Previous output was restored but transaction cleanup failed.",
                        owned.staging));
                }
            }
            else if (!owned.hadPrevious)
            {
                result.state = StandalonePublicationRecoveryState::FailedClosed;
                result.diagnostics.push_back(Diagnostic(
                    "Build.Publish.RecoveryCandidateAmbiguous",
                    "Candidate publication state is missing or ambiguous.", owned.staging));
            }
            return result;
        }
        if (!error) WriteJournal(owned.journalPath, owned.json, "CandidatePublished", error);
        owned.phase = "CandidatePublished";
    }
    if (error || owned.phase != "CandidatePublished" ||
        !VerifyManifest(owned.destination, owned.snapshotId, owned.manifestHash) ||
        !VerifyTree(owned.destination, owned.candidateInventory))
    {
        if (!error && Restore(owned, result))
        {
            fs::remove_all(owned.staging, error);
            if (error)
            {
                result.state = StandalonePublicationRecoveryState::FailedClosed;
                result.diagnostics.push_back(Diagnostic("Build.Publish.RecoveryCleanupFailed",
                    "Previous output was restored but transaction cleanup failed.",
                    owned.staging));
            }
        }
        else if (error)
        {
            result.state = StandalonePublicationRecoveryState::FailedClosed;
            result.diagnostics.push_back(Diagnostic("Build.Publish.RecoveryPublishFailed",
                error.message(), owned.staging));
        }
        return result;
    }
    if (!WriteJournal(owned.journalPath, owned.json, "Completed", error))
    {
        result.state = StandalonePublicationRecoveryState::FailedClosed;
        result.diagnostics.push_back(Diagnostic("Build.Publish.RecoveryJournalFailed",
            error.message(), owned.journalPath));
        return result;
    }
    fs::remove_all(owned.staging, error);
    if (error)
    {
        result.state = StandalonePublicationRecoveryState::FailedClosed;
        result.diagnostics.push_back(Diagnostic("Build.Publish.RecoveryCleanupFailed",
            "Published output is valid but transaction cleanup failed.", owned.staging));
        return result;
    }
    result.state = StandalonePublicationRecoveryState::CandidatePublished;
    result.recovered = true;
    return result;
}
} // namespace

StandalonePublicationRecoveryResult StandalonePublicationTransaction::RecoverOwned(
    const fs::path& outputDirectory)
{
    StandalonePublicationRecoveryResult aggregate;
    std::error_code error;
    const auto transactionRoot = fs::absolute(outputDirectory.parent_path()).lexically_normal() /
        NamespaceName / DestinationIdentity(outputDirectory);
    if (!fs::is_directory(transactionRoot, error)) return aggregate;
    if (error || fs::is_symlink(transactionRoot, error))
    {
        aggregate.state = StandalonePublicationRecoveryState::FailedClosed;
        aggregate.retainedEvidence = true;
        aggregate.terminalFailure = true;
        aggregate.diagnostics.push_back(Diagnostic("Build.Publish.RecoveryNamespaceUntrusted",
            "The destination publication namespace is aliased or unreadable.", transactionRoot));
        return aggregate;
    }
    std::vector<fs::path> journals;
    for (const auto& entry : fs::directory_iterator(transactionRoot, error))
    {
        if (error || !entry.is_directory() || entry.is_symlink() ||
            !fs::is_regular_file(entry.path() / JournalName, error))
        {
            aggregate.state = StandalonePublicationRecoveryState::FailedClosed;
            aggregate.retainedEvidence = true;
            aggregate.terminalFailure = true;
            aggregate.diagnostics.push_back(Diagnostic("Build.Publish.RecoveryResidueUntrusted",
                "Unexpected residue exists in the destination publication namespace.",
                entry.path()));
            return aggregate;
        }
        journals.push_back(entry.path() / JournalName);
    }
    if (error)
    {
        aggregate.state = StandalonePublicationRecoveryState::FailedClosed;
        aggregate.retainedEvidence = true;
        aggregate.terminalFailure = true;
        aggregate.diagnostics.push_back(Diagnostic("Build.Publish.RecoveryNamespaceUnreadable",
            error.message(), transactionRoot));
        return aggregate;
    }
    std::sort(journals.begin(), journals.end());
    if (journals.size() > 1)
    {
        aggregate.state = StandalonePublicationRecoveryState::FailedClosed;
        aggregate.retainedEvidence = true;
        aggregate.terminalFailure = true;
        aggregate.diagnostics.push_back(Diagnostic("Build.Publish.RecoveryAmbiguous",
            "More than one owned publication transaction exists for this destination.",
            transactionRoot));
        return aggregate;
    }
    for (const auto& journal : journals)
    {
        auto current = RecoverJournal(outputDirectory, journal);
        if (!current.Succeeded())
        {
            current.retainedEvidence = true;
            current.terminalFailure = true;
        }
        aggregate.diagnostics.insert(aggregate.diagnostics.end(),
            current.diagnostics.begin(), current.diagnostics.end());
        if (!current.Succeeded()) return current;
        if (current.recovered) aggregate = std::move(current);
    }
    return aggregate;
}

fs::path StandalonePublicationTransaction::OwnedTransactionDirectory(
    const fs::path& outputDirectory, std::string_view transactionId)
{
    if (!IsHash(transactionId)) return {};
    return TransactionDirectory(outputDirectory, transactionId);
}

StandalonePublicationResult StandalonePublicationTransaction::Publish(
    const StandalonePublicationRequest& request)
{
    StandalonePublicationResult result;
    const auto journalPath = request.stagingDirectory / JournalName;
    const auto backup = request.stagingDirectory / "previous";
    Inventory candidateInventory, previousInventory;
    const auto expectedStaging = TransactionDirectory(
        request.destinationDirectory, request.snapshotId);
    std::error_code error;
    if (!IsHash(request.snapshotId) || !IsHash(request.manifestHash) ||
        expectedStaging.empty() ||
        fs::absolute(request.stagingDirectory).lexically_normal() != expectedStaging ||
        fs::absolute(request.candidateDirectory).lexically_normal() !=
            expectedStaging / "candidate" ||
        HasSymlink(fs::absolute(request.destinationDirectory.parent_path()).lexically_normal(),
            expectedStaging) || request.destinationDirectory.filename().empty() ||
        request.destinationDirectory.filename() == "." ||
        request.destinationDirectory.filename() == "..")
    {
        result.diagnostics.push_back(Diagnostic("Build.Publish.OwnershipInvalid",
            "Publication staging is outside the destination-keyed Saga namespace.",
            request.stagingDirectory));
        return result;
    }
    if (!VerifyManifest(request.candidateDirectory, request.snapshotId, request.manifestHash) ||
        !InventoryTree(request.candidateDirectory, candidateInventory))
    {
        result.diagnostics.push_back(Diagnostic("Build.Publish.CandidateInvalid",
            "Candidate manifest or artifact inventory failed verification.",
            request.candidateDirectory));
        return result;
    }
    const bool hadPrevious = fs::exists(request.destinationDirectory, error);
    const bool previousValid = hadPrevious &&
        VerifyExistingManifest(request.destinationDirectory) &&
        InventoryTree(request.destinationDirectory, previousInventory);
    if (hadPrevious && !previousValid)
    {
        result.diagnostics.push_back(Diagnostic("Build.Publish.PreviousOutputUnverified",
            "Existing output is not independently verifiable; publication made no mutation.",
            request.destinationDirectory));
        fs::remove_all(request.stagingDirectory, error);
        if (error)
        {
            result.recovery.state = StandalonePublicationRecoveryState::FailedClosed;
            result.recovery.retainedEvidence = true;
            result.recovery.terminalFailure = true;
            result.diagnostics.push_back(Diagnostic(
                "Build.Publish.PreMutationCleanupFailed",
                "Publication made no destination mutation, but owned candidate cleanup failed.",
                request.stagingDirectory));
        }
        return result;
    }
    Json journal{{"backup", "previous"}, {"candidate", "candidate"},
        {"candidateInventory", InventoryJson(candidateInventory)},
        {"destinationIdentity", DestinationIdentity(request.destinationDirectory)},
        {"destinationName", request.destinationDirectory.filename().generic_string()},
        {"hadPrevious", hadPrevious}, {"manifestHash", request.manifestHash},
        {"owner", Owner}, {"previousInventory", InventoryJson(previousInventory)},
        {"previousVerified", previousValid},
        {"schemaVersion", 2}, {"snapshotId", request.snapshotId}};
    if (!WriteJournal(journalPath, journal, "Prepared", error))
    {
        result.diagnostics.push_back(Diagnostic("Build.Publish.JournalWriteFailed",
            error.message(), journalPath)); return result;
    }
    const auto interrupted = [&](StandalonePublicationBoundary boundary)
    {
        if (!request.transitionAllowed || request.transitionAllowed(boundary)) return false;
        result.recovery.retainedEvidence = true;
        result.diagnostics.push_back(Diagnostic("Build.Publish.Interrupted",
            "Publication was interrupted at a deterministic journal boundary.", journalPath));
        return true;
    };
    if (interrupted(StandalonePublicationBoundary::Prepared)) return result;
    if (hadPrevious) fs::rename(request.destinationDirectory, backup, error);
    if (!error && interrupted(StandalonePublicationBoundary::PreviousMoveCompleted))
        return result;
    if (!error && !WriteJournal(journalPath, journal, "PreviousMoved", error)) {}
    if (!error && interrupted(StandalonePublicationBoundary::PreviousMoved)) return result;
    if (!error && request.publicationAllowed && !request.publicationAllowed())
    {
        result.cancelled = true;
        error = std::make_error_code(std::errc::operation_canceled);
    }
    if (!error) fs::rename(request.candidateDirectory, request.destinationDirectory, error);
    if (!error && interrupted(StandalonePublicationBoundary::CandidateMoveCompleted))
        return result;
    if (!error && !WriteJournal(journalPath, journal, "CandidatePublished", error)) {}
    if (!error && interrupted(StandalonePublicationBoundary::CandidatePublished)) return result;
    if (!error && (!VerifyManifest(request.destinationDirectory,
            request.snapshotId, request.manifestHash) ||
        !VerifyTree(request.destinationDirectory, candidateInventory)))
        error = std::make_error_code(std::errc::state_not_recoverable);
    if (error)
    {
        OwnedJournal owned{journalPath, request.stagingDirectory, request.candidateDirectory,
            request.destinationDirectory, backup, request.snapshotId, request.manifestHash,
            journal.value("phase", "Prepared"), candidateInventory, previousInventory,
            hadPrevious, previousValid, journal};
        if (hadPrevious && previousValid)
        {
            StandalonePublicationRecoveryResult recovery;
            (void)Restore(owned, recovery);
            if (!recovery.Succeeded())
            {
                recovery.retainedEvidence = true;
                recovery.terminalFailure = true;
            }
            else
            {
                std::error_code cleanupError;
                fs::remove_all(request.stagingDirectory, cleanupError);
                if (cleanupError)
                {
                    recovery.state = StandalonePublicationRecoveryState::FailedClosed;
                    recovery.retainedEvidence = true;
                    recovery.terminalFailure = true;
                    recovery.diagnostics.push_back(Diagnostic(
                        "Build.Publish.RestoreCleanupFailed",
                        "Previous output was restored but transaction cleanup failed.",
                        request.stagingDirectory));
                }
            }
            result.preservedPreviousOutput = recovery.preservedPreviousOutput;
            recovery.cancellationOutcome = result.cancelled && recovery.Succeeded()
                ? StandalonePublicationCancellationOutcome::PreviousOutputRestored
                : (result.cancelled
                    ? StandalonePublicationCancellationOutcome::EvidenceRetained
                    : StandalonePublicationCancellationOutcome::None);
            result.recovery = std::move(recovery);
        }
        else if (result.cancelled)
        {
            result.recovery.cancellationOutcome =
                StandalonePublicationCancellationOutcome::BeforeCandidatePublication;
            std::error_code cleanupError;
            fs::remove_all(request.stagingDirectory, cleanupError);
            if (cleanupError)
            {
                result.recovery.state = StandalonePublicationRecoveryState::FailedClosed;
                result.recovery.retainedEvidence = true;
                result.recovery.terminalFailure = true;
                result.recovery.cancellationOutcome =
                    StandalonePublicationCancellationOutcome::EvidenceRetained;
            }
        }
        result.diagnostics.push_back(Diagnostic("Build.Publish.Failed",
            result.preservedPreviousOutput ?
                "Publication failed and the verified previous output was restored." :
                "Publication failed; no verified previous output preservation is claimed.",
            request.destinationDirectory));
        return result;
    }
    if (!WriteJournal(journalPath, journal, "Completed", error))
    {
        result.diagnostics.push_back(Diagnostic("Build.Publish.CompletionJournalFailed",
            error.message(), journalPath)); return result;
    }
    if (interrupted(StandalonePublicationBoundary::Completed)) return result;
    fs::remove_all(request.stagingDirectory, error);
    if (error)
    {
        result.recovery.state = StandalonePublicationRecoveryState::FailedClosed;
        result.recovery.retainedEvidence = true;
        result.recovery.terminalFailure = true;
        result.diagnostics.push_back(Diagnostic("Build.Publish.CleanupFailed",
            "Published output is valid but owned transaction cleanup failed.",
            request.stagingDirectory));
        return result;
    }
    result.published = true;
    result.preservedPreviousOutput = false;
    return result;
}
} // namespace SagaEditor
