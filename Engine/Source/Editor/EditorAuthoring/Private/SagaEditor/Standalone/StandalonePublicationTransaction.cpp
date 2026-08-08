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
constexpr std::string_view Owner = "SagaEditor.StandalonePublicationTransaction/v1";
constexpr std::string_view JournalName = "publish-journal.json";

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
        if (!entry.is_regular_file()) continue;
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
        if (fs::path(path).is_absolute() || path.empty() || path.starts_with("../") ||
            !hashValue.is_string() || !IsHash(hashValue.get<std::string>())) return false;
        inventory.emplace(path, hashValue.get<std::string>());
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
    Json journal;
    if (!ReadJson(journalPath, journal) || journal.value("schemaVersion", 0) != 1 ||
        journal.value("owner", "") != Owner || journal.value("destination", "") !=
            fs::absolute(output).lexically_normal().string()) return false;
    const auto parent = fs::absolute(output.parent_path()).lexically_normal();
    owned.journalPath = journalPath;
    owned.staging = fs::absolute(journalPath.parent_path()).lexically_normal();
    owned.candidate = fs::path(journal.value("candidate", "")).lexically_normal();
    owned.destination = fs::path(journal.value("destination", "")).lexically_normal();
    owned.backup = fs::path(journal.value("backup", "")).lexically_normal();
    owned.snapshotId = journal.value("snapshotId", "");
    owned.manifestHash = journal.value("manifestHash", "");
    owned.phase = journal.value("phase", "");
    owned.hadPrevious = journal.value("hadPrevious", false);
    owned.previousVerified = journal.value("previousVerified", false);
    owned.json = std::move(journal);
    if (owned.staging.parent_path() != parent ||
        !owned.staging.filename().string().starts_with(".saga-standalone-") ||
        !LexicallyContained(owned.staging, owned.candidate) ||
        !LexicallyContained(owned.staging, owned.backup) ||
        HasSymlink(parent, owned.staging) || HasSymlink(owned.staging, owned.candidate) ||
        HasSymlink(owned.staging, owned.backup) || !IsHash(owned.snapshotId) ||
        !IsHash(owned.manifestHash) ||
        !ParseInventory(owned.json["candidateInventory"], owned.candidateInventory)) return false;
    if (owned.previousVerified &&
        !ParseInventory(owned.json["previousInventory"], owned.previousInventory)) return false;
    return owned.phase == "Prepared" || owned.phase == "PreviousMoved" ||
        owned.phase == "CandidatePublished" || owned.phase == "Completed";
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
        result.state = StandalonePublicationRecoveryState::Completed;
        result.recovered = true;
        return result;
    }

    if (owned.phase == "Prepared" && owned.hadPrevious)
    {
        if (!owned.previousVerified ||
            !VerifyExistingManifest(owned.destination) ||
            !VerifyTree(owned.destination, owned.previousInventory) ||
            !VerifyTree(owned.candidate, owned.candidateInventory))
        {
            result.state = StandalonePublicationRecoveryState::FailedClosed;
            result.diagnostics.push_back(Diagnostic("Build.Publish.PreparedInvalid",
                "Prepared publication inputs changed; transaction was retained.", owned.staging));
            return result;
        }
        fs::rename(owned.destination, owned.backup, error);
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
        if (!VerifyTree(owned.candidate, owned.candidateInventory))
        {
            if (Restore(owned, result)) fs::remove_all(owned.staging, error);
            return result;
        }
        fs::rename(owned.candidate, owned.destination, error);
        if (!error) WriteJournal(owned.journalPath, owned.json, "CandidatePublished", error);
        owned.phase = "CandidatePublished";
    }
    if (error || owned.phase != "CandidatePublished" ||
        !VerifyManifest(owned.destination, owned.snapshotId, owned.manifestHash) ||
        !VerifyTree(owned.destination, owned.candidateInventory))
    {
        if (!error && Restore(owned, result)) fs::remove_all(owned.staging, error);
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
    const auto parent = outputDirectory.parent_path();
    if (!fs::is_directory(parent, error)) return aggregate;
    std::vector<fs::path> journals;
    for (const auto& entry : fs::directory_iterator(parent, error))
        if (!error && entry.is_directory() &&
            entry.path().filename().string().starts_with(".saga-standalone-") &&
            fs::exists(entry.path() / JournalName))
            journals.push_back(entry.path() / JournalName);
    std::sort(journals.begin(), journals.end());
    for (const auto& journal : journals)
    {
        auto current = RecoverJournal(outputDirectory, journal);
        aggregate.diagnostics.insert(aggregate.diagnostics.end(),
            current.diagnostics.begin(), current.diagnostics.end());
        if (!current.Succeeded()) { aggregate.state = current.state; return aggregate; }
        if (current.recovered) aggregate = std::move(current);
    }
    return aggregate;
}

StandalonePublicationResult StandalonePublicationTransaction::Publish(
    const StandalonePublicationRequest& request)
{
    StandalonePublicationResult result;
    const auto journalPath = request.stagingDirectory / JournalName;
    const auto backup = request.stagingDirectory / "previous";
    Inventory candidateInventory, previousInventory;
    if (!VerifyManifest(request.candidateDirectory, request.snapshotId, request.manifestHash) ||
        !InventoryTree(request.candidateDirectory, candidateInventory))
    {
        result.diagnostics.push_back(Diagnostic("Build.Publish.CandidateInvalid",
            "Candidate manifest or artifact inventory failed verification.",
            request.candidateDirectory));
        return result;
    }
    std::error_code error;
    const bool hadPrevious = fs::exists(request.destinationDirectory, error);
    const bool previousValid = hadPrevious &&
        VerifyExistingManifest(request.destinationDirectory) &&
        InventoryTree(request.destinationDirectory, previousInventory);
    Json journal{{"backup", fs::absolute(backup).lexically_normal().string()},
        {"candidate", fs::absolute(request.candidateDirectory).lexically_normal().string()},
        {"candidateInventory", InventoryJson(candidateInventory)},
        {"destination", fs::absolute(request.destinationDirectory).lexically_normal().string()},
        {"hadPrevious", hadPrevious}, {"manifestHash", request.manifestHash},
        {"owner", Owner}, {"previousInventory", InventoryJson(previousInventory)},
        {"previousVerified", previousValid},
        {"schemaVersion", 1}, {"snapshotId", request.snapshotId}};
    if (!WriteJournal(journalPath, journal, "Prepared", error))
    {
        result.diagnostics.push_back(Diagnostic("Build.Publish.JournalWriteFailed",
            error.message(), journalPath)); return result;
    }
    if (hadPrevious) fs::rename(request.destinationDirectory, backup, error);
    if (!error && !WriteJournal(journalPath, journal, "PreviousMoved", error)) {}
    if (!error && request.publicationAllowed && !request.publicationAllowed())
        error = std::make_error_code(std::errc::operation_canceled);
    if (!error) fs::rename(request.candidateDirectory, request.destinationDirectory, error);
    if (!error && !WriteJournal(journalPath, journal, "CandidatePublished", error)) {}
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
            result.preservedPreviousOutput = recovery.preservedPreviousOutput;
            result.recovery = std::move(recovery);
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
    result.published = true;
    result.preservedPreviousOutput = false;
    return result;
}
} // namespace SagaEditor
