// SPDX-License-Identifier: MPL-2.0

#include "SagaEditor/Project/ProjectMutation.h"

#include "SagaEditor/Documents/DocumentPersistence.h"

#include <nlohmann/json.hpp>
#include <openssl/sha.h>

#include <array>
#include <algorithm>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <random>
#include <sstream>
#include <system_error>

#ifndef _WIN32
#include <cerrno>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>
#else
#include <windows.h>
#endif

namespace SagaEditor
{
namespace
{
namespace fs = std::filesystem;

ProjectMutationResult Ok() { return {true, {}, {}}; }
ProjectMutationResult Fail(std::string code, std::string message)
{ return {false, std::move(code), std::move(message)}; }

std::string ReadBytes(const fs::path& path, std::error_code& error)
{
    error.clear();
    std::ifstream input(path, std::ios::binary);
    if (!input)
    {
        error = std::make_error_code(std::errc::no_such_file_or_directory);
        return {};
    }
    std::ostringstream output;
    output << input.rdbuf();
    if (!input.good() && !input.eof())
        error = std::make_error_code(std::errc::io_error);
    return output.str();
}

std::string Hash(std::string_view bytes)
{
    std::array<unsigned char, SHA256_DIGEST_LENGTH> digest{};
    SHA256(reinterpret_cast<const unsigned char*>(bytes.data()), bytes.size(), digest.data());
    std::ostringstream text;
    text << "sha256:";
    for (const auto value : digest)
        text << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(value);
    return text.str();
}

std::string HashFile(const fs::path& path, std::error_code& error)
{ return Hash(ReadBytes(path, error)); }

std::string Identity(const fs::path& path, std::error_code& error)
{
#ifndef _WIN32
    struct stat state {};
    if (::stat(path.c_str(), &state) != 0)
    {
        error = std::error_code(errno, std::generic_category());
        return {};
    }
    if (!S_ISREG(state.st_mode))
    {
        error = std::make_error_code(std::errc::invalid_argument);
        return {};
    }
    error.clear();
    return std::to_string(static_cast<unsigned long long>(state.st_dev)) + ":" +
        std::to_string(static_cast<unsigned long long>(state.st_ino)) + ":" +
        std::to_string(static_cast<unsigned long long>(state.st_size)) + ":" +
        std::to_string(static_cast<long long>(state.st_mtim.tv_sec)) + ":" +
        std::to_string(static_cast<long long>(state.st_mtim.tv_nsec));
#else
    WIN32_FILE_ATTRIBUTE_DATA state{};
    if (!GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &state))
    {
        error = std::error_code(static_cast<int>(GetLastError()), std::system_category());
        return {};
    }
    if ((state.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
    {
        error = std::make_error_code(std::errc::invalid_argument);
        return {};
    }
    error.clear();
    return std::to_string(state.nFileSizeHigh) + ":" + std::to_string(state.nFileSizeLow) +
        ":" + std::to_string(state.ftLastWriteTime.dwHighDateTime) + ":" +
        std::to_string(state.ftLastWriteTime.dwLowDateTime);
#endif
}

bool Within(const fs::path& root, const fs::path& path)
{
    const auto relative = path.lexically_relative(root);
    return !relative.empty() && !relative.is_absolute() &&
        *relative.begin() != "..";
}

std::string Nonce()
{
    std::random_device device;
    std::mt19937_64 generator(device());
    std::ostringstream value;
    value << std::hex << generator() << generator();
    return value.str();
}

std::string Now()
{
    return std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count());
}

std::string PhaseName(ProjectMutationPhase phase)
{
    switch (phase)
    {
    case ProjectMutationPhase::Preparing: return "Preparing";
    case ProjectMutationPhase::Resolved: return "Resolved";
    case ProjectMutationPhase::Reviewed: return "Reviewed";
    case ProjectMutationPhase::Staging: return "Staging";
    case ProjectMutationPhase::CommitMarked: return "CommitMarked";
    case ProjectMutationPhase::Publishing: return "Publishing";
    case ProjectMutationPhase::Committed: return "Committed";
    case ProjectMutationPhase::Cancelled: return "Cancelled";
    case ProjectMutationPhase::RolledBack: return "RolledBack";
    case ProjectMutationPhase::RecoveryRequired: return "RecoveryRequired";
    }
    return "RecoveryRequired";
}

ProjectMutationPhase ParsePhase(const std::string& phase)
{
    if (phase == "Preparing") return ProjectMutationPhase::Preparing;
    if (phase == "Resolved") return ProjectMutationPhase::Resolved;
    if (phase == "Reviewed") return ProjectMutationPhase::Reviewed;
    if (phase == "Staging") return ProjectMutationPhase::Staging;
    if (phase == "CommitMarked") return ProjectMutationPhase::CommitMarked;
    if (phase == "Publishing") return ProjectMutationPhase::Publishing;
    if (phase == "Committed") return ProjectMutationPhase::Committed;
    if (phase == "Cancelled") return ProjectMutationPhase::Cancelled;
    if (phase == "RolledBack") return ProjectMutationPhase::RolledBack;
    return ProjectMutationPhase::RecoveryRequired;
}

fs::path CoordinationRoot(const fs::path& root)
{ return root / ".saga/local/transactions/package"; }

std::string ProcessIdentity()
{
#ifndef _WIN32
    std::error_code error;
    const auto boot = ReadBytes("/proc/sys/kernel/random/boot_id", error);
    if (error) return {};
    std::ifstream stat("/proc/self/stat");
    std::string line;
    std::getline(stat, line);
    const auto close = line.rfind(')');
    if (close == std::string::npos) return {};
    std::istringstream fields(line.substr(close + 2));
    std::string field;
    for (int index = 3; index <= 22; ++index)
        if (!(fields >> field)) return {};
    return boot.substr(0, boot.find_first_of("\r\n")) + ":" + field;
#else
    FILETIME creation{}, exit{}, kernel{}, user{};
    if (!GetProcessTimes(GetCurrentProcess(), &creation, &exit, &kernel, &user)) return {};
    ULARGE_INTEGER ticks{};
    ticks.HighPart = creation.dwHighDateTime;
    ticks.LowPart = creation.dwLowDateTime;
    return std::to_string(ticks.QuadPart);
#endif
}

nlohmann::json OwnerJson(const ProjectMutationOwner& owner)
{
    return {{"pid", owner.pid}, {"processIdentity", owner.processIdentity},
        {"nonce", owner.nonce}, {"mutationKind", owner.mutationKind},
        {"projectHash", owner.projectHash}, {"acquiredAt", owner.acquiredAt}};
}
}

struct ProjectMutationLease::State
{
    fs::path root;
    fs::path lockPath;
    ProjectMutationOwner owner;
#ifndef _WIN32
    int descriptor = -1;
#else
    HANDLE handle = INVALID_HANDLE_VALUE;
    OVERLAPPED overlap{};
#endif
};

ProjectMutationLease::ProjectMutationLease() = default;
ProjectMutationLease::ProjectMutationLease(std::unique_ptr<State> state)
    : state_(std::move(state)) {}
ProjectMutationLease::ProjectMutationLease(ProjectMutationLease&&) noexcept = default;
ProjectMutationLease& ProjectMutationLease::operator=(ProjectMutationLease&&) noexcept = default;
ProjectMutationLease::~ProjectMutationLease()
{
    if (!state_) return;
#ifndef _WIN32
    if (state_->descriptor >= 0)
    {
        (void)::flock(state_->descriptor, LOCK_UN);
        (void)::close(state_->descriptor);
    }
#else
    if (state_->handle != INVALID_HANDLE_VALUE)
    {
        (void)UnlockFileEx(state_->handle, 0, MAXDWORD, MAXDWORD, &state_->overlap);
        (void)CloseHandle(state_->handle);
    }
#endif
}

std::pair<ProjectMutationLease, ProjectMutationResult>
ProjectMutationLease::Acquire(const fs::path& projectRoot, std::string mutationKind)
{
    std::error_code error;
    const auto canonical = fs::canonical(projectRoot, error);
    if (error || !fs::is_directory(canonical))
        return {ProjectMutationLease{}, Fail("Project.Mutation.InvalidRoot",
            "Project mutation requires an existing canonical project directory.")};
    const auto identity = ProcessIdentity();
    if (identity.empty())
        return {ProjectMutationLease{}, Fail("Project.Mutation.ProcessIdentityUnavailable",
            "The platform could not provide a process creation identity.")};
    const auto coordination = CoordinationRoot(canonical);
    fs::create_directories(coordination, error);
    if (error)
        return {ProjectMutationLease{},
            Fail("Project.Mutation.CoordinationUnavailable", error.message())};

    auto state = std::make_unique<State>();
    state->root = canonical;
    state->lockPath = coordination / "writer.lock";
    state->owner.pid =
#ifndef _WIN32
        std::to_string(::getpid());
#else
        std::to_string(GetCurrentProcessId());
#endif
    state->owner.processIdentity = identity;
    state->owner.nonce = Nonce();
    state->owner.mutationKind = std::move(mutationKind);
    state->owner.projectHash = Hash(canonical.generic_string());
    state->owner.acquiredAt = Now();
#ifndef _WIN32
    state->descriptor = ::open(state->lockPath.c_str(), O_RDWR | O_CREAT, 0600);
    if (state->descriptor < 0)
        return {ProjectMutationLease{}, Fail("Project.Mutation.LeaseContended",
            "Another writer owns the canonical project mutation lease.")};
    if (::flock(state->descriptor, LOCK_EX | LOCK_NB) != 0)
    {
        (void)::close(state->descriptor);
        state->descriptor = -1;
        return {ProjectMutationLease{}, Fail("Project.Mutation.LeaseContended",
            "Another writer owns the canonical project mutation lease.")};
    }
    const auto metadata = OwnerJson(state->owner).dump(2) + "\n";
    if (::ftruncate(state->descriptor, 0) != 0 ||
        ::write(state->descriptor, metadata.data(), metadata.size()) !=
            static_cast<ssize_t>(metadata.size()) ||
        ::fsync(state->descriptor) != 0)
    {
        (void)::flock(state->descriptor, LOCK_UN);
        (void)::close(state->descriptor);
        state->descriptor = -1;
        return {ProjectMutationLease{}, Fail("Project.Mutation.LeaseMetadataFailed",
            "The writer lease metadata could not be made durable.")};
    }
#else
    state->handle = CreateFileW(state->lockPath.c_str(), GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_ALWAYS,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH, nullptr);
    if (state->handle == INVALID_HANDLE_VALUE ||
        !LockFileEx(state->handle, LOCKFILE_EXCLUSIVE_LOCK | LOCKFILE_FAIL_IMMEDIATELY,
            0, MAXDWORD, MAXDWORD, &state->overlap))
    {
        if (state->handle != INVALID_HANDLE_VALUE) (void)CloseHandle(state->handle);
        state->handle = INVALID_HANDLE_VALUE;
        return {ProjectMutationLease{}, Fail("Project.Mutation.LeaseContended",
            "Another writer owns the canonical project mutation lease.")};
    }
    const auto metadata = OwnerJson(state->owner).dump(2) + "\n";
    DWORD written = 0;
    SetFilePointer(state->handle, 0, nullptr, FILE_BEGIN);
    SetEndOfFile(state->handle);
    if (!WriteFile(state->handle, metadata.data(), static_cast<DWORD>(metadata.size()),
            &written, nullptr) || written != metadata.size() || !FlushFileBuffers(state->handle))
    {
        (void)UnlockFileEx(state->handle, 0, MAXDWORD, MAXDWORD, &state->overlap);
        (void)CloseHandle(state->handle);
        state->handle = INVALID_HANDLE_VALUE;
        return {ProjectMutationLease{}, Fail("Project.Mutation.LeaseMetadataFailed",
            "The writer lease metadata could not be made durable.")};
    }
#endif
    return {ProjectMutationLease(std::move(state)), Ok()};
}

bool ProjectMutationLease::Held() const noexcept { return state_ != nullptr; }
bool ProjectMutationLease::Revalidate() const
{
    if (!state_ || state_->owner.processIdentity != ProcessIdentity()) return false;
    std::error_code error;
    return fs::canonical(state_->root, error) == state_->root && !error;
}
const fs::path& ProjectMutationLease::ProjectRoot() const noexcept
{
    static const fs::path empty;
    return state_ ? state_->root : empty;
}
const ProjectMutationOwner& ProjectMutationLease::Owner() const noexcept
{
    static const ProjectMutationOwner empty;
    return state_ ? state_->owner : empty;
}

ProjectMutationResult ProjectMutationJournal::Save(const fs::path& journalPath) const
{
    nlohmann::json filesJson = nlohmann::json::array();
    for (const auto& file : files)
        filesJson.push_back({{"target", file.relativeTarget.generic_string()},
            {"staged", file.stagedPath.generic_string()}, {"oldHash", file.oldHash},
            {"newHash", file.newHash}, {"oldIdentity", file.oldIdentity},
            {"stagedIdentity", file.stagedIdentity}, {"published", file.published}});
    const nlohmann::json value{{"schemaVersion", schemaVersion},
        {"transactionId", transactionId}, {"operationId", operationId},
        {"projectIdentity", projectIdentity}, {"toolVersion", toolVersion},
        {"owner", OwnerJson(owner)}, {"phase", PhaseName(phase)},
        {"commitMarker", commitMarker}, {"files", std::move(filesJson)}};
    const auto result = DefaultDocumentPersistence()->Publish(
        journalPath, value.dump(2) + "\n", transactionId);
    if (!result.renamed || result.error)
        return Fail("Project.Mutation.JournalWriteFailed", result.error.message());
    return Ok();
}

std::pair<std::optional<ProjectMutationJournal>, ProjectMutationResult>
ProjectMutationJournal::Load(const fs::path& path)
{
    try
    {
        std::error_code error;
        const auto bytes = ReadBytes(path, error);
        if (error) return {std::nullopt,
            Fail("Project.Mutation.JournalReadFailed", error.message())};
        const auto value = nlohmann::json::parse(bytes);
        ProjectMutationJournal journal;
        journal.schemaVersion = value.at("schemaVersion").get<int>();
        if (journal.schemaVersion > CurrentSchemaVersion)
            return {std::nullopt, Fail("Project.Mutation.UnsupportedJournal",
                "A newer transaction journal schema must be preserved.")};
        if (journal.schemaVersion != CurrentSchemaVersion)
            return {std::nullopt, Fail("Project.Mutation.InvalidJournal",
                "The transaction journal schema is invalid.")};
        journal.transactionId = value.at("transactionId").get<std::string>();
        journal.operationId = value.at("operationId").get<std::string>();
        journal.projectIdentity = value.at("projectIdentity").get<std::string>();
        journal.toolVersion = value.at("toolVersion").get<std::string>();
        journal.phase = ParsePhase(value.at("phase").get<std::string>());
        journal.commitMarker = value.at("commitMarker").get<bool>();
        const auto& owner = value.at("owner");
        journal.owner = {owner.at("pid").get<std::string>(),
            owner.at("processIdentity").get<std::string>(),
            owner.at("nonce").get<std::string>(),
            owner.at("mutationKind").get<std::string>(),
            owner.at("projectHash").get<std::string>(),
            owner.at("acquiredAt").get<std::string>()};
        for (const auto& entry : value.at("files"))
            journal.files.push_back({entry.at("target").get<std::string>(),
                entry.at("staged").get<std::string>(),
                entry.at("oldHash").get<std::string>(),
                entry.at("newHash").get<std::string>(),
                entry.at("oldIdentity").get<std::string>(),
                entry.at("stagedIdentity").get<std::string>(),
                entry.at("published").get<bool>()});
        return {journal, Ok()};
    }
    catch (const std::exception& exception)
    {
        return {std::nullopt,
            Fail("Project.Mutation.InvalidJournal", exception.what())};
    }
}

ProjectMutationTransaction::ProjectMutationTransaction(ProjectMutationLease& lease,
    std::string operationId, std::string toolVersion,
    ProjectMutationFaultInjector injector)
    : lease_(lease), injector_(std::move(injector))
{
    journal_.transactionId = Nonce();
    journal_.operationId = std::move(operationId);
    journal_.projectIdentity = Hash(lease.ProjectRoot().generic_string());
    journal_.toolVersion = std::move(toolVersion);
    journal_.owner = lease.Owner();
    transactionRoot_ = CoordinationRoot(lease.ProjectRoot()) / journal_.transactionId;
    journalPath_ = transactionRoot_ / "journal.v1.json";
}

ProjectMutationResult ProjectMutationTransaction::Persist()
{ return journal_.Save(journalPath_); }

ProjectMutationResult ProjectMutationTransaction::FailBeforeCommit(
    std::string code, std::string message)
{
    std::error_code cleanupError;
    fs::remove_all(transactionRoot_, cleanupError);
    journal_.phase = ProjectMutationPhase::Cancelled;
    if (cleanupError)
        return Fail("Project.Mutation.CleanupFailed", cleanupError.message());
    return Fail(std::move(code), std::move(message));
}

ProjectMutationResult ProjectMutationTransaction::EnterRecoveryRequired(
    std::string code, std::string message)
{
    journal_.phase = ProjectMutationPhase::RecoveryRequired;
    const auto persisted = Persist();
    if (!persisted.succeeded)
        return Fail("Project.Mutation.RecoveryJournalWriteFailed",
            code + ": " + message + "; " + persisted.message);
    return Fail(std::move(code), std::move(message));
}

ProjectMutationResult ProjectMutationTransaction::RevalidateReviewedInventory(
    bool publicationStarted) const
{
    if (!lease_.Revalidate())
        return Fail("Project.Mutation.LeaseLost",
            "The writer lease is no longer valid.");
    std::error_code error;
    for (const auto& file : journal_.files)
    {
        const auto target = lease_.ProjectRoot() / file.relativeTarget;
        const auto expectedHash = file.published ? file.newHash : file.oldHash;
        if (HashFile(target, error) != expectedHash || error)
            return Fail(publicationStarted
                    ? "Project.Mutation.ExternalPublicationConflict"
                    : "Project.Mutation.StaleReview",
                file.published
                    ? "An already-published target no longer contains the reviewed new bytes."
                    : "A reviewed canonical file changed before publication.");
        if (!file.published &&
            (Identity(target, error) != file.oldIdentity || error))
            return Fail(publicationStarted
                    ? "Project.Mutation.ExternalPublicationConflict"
                    : "Project.Mutation.StaleReview",
                "A reviewed canonical file identity changed before publication.");
        const auto staged = lease_.ProjectRoot() / file.stagedPath;
        if (HashFile(staged, error) != file.newHash || error ||
            Identity(staged, error) != file.stagedIdentity || error)
            return Fail("Project.Mutation.StagedFileChanged",
                "A reviewed staged file changed before publication.");
    }
    return Ok();
}

ProjectMutationResult ProjectMutationTransaction::Stage(
    const std::vector<CandidateFile>& candidates)
{
    if (!lease_.Revalidate())
        return Fail("Project.Mutation.LeaseLost", "The writer lease is no longer valid.");
    if (journal_.phase != ProjectMutationPhase::Preparing || candidates.empty())
        return Fail("Project.Mutation.InvalidState", "Staging is not valid in the current phase.");
    std::error_code error;
    fs::create_directories(transactionRoot_ / "staged", error);
    if (error) return Fail("Project.Mutation.StagingUnavailable", error.message());
    std::size_t index = 0;
    for (const auto& candidate : candidates)
    {
        if (candidate.relativeTarget.empty() || candidate.relativeTarget.is_absolute() ||
            candidate.relativeTarget.lexically_normal().generic_string().starts_with("../"))
            return FailBeforeCommit("Project.Mutation.TargetEscapesProject",
                "A canonical target escaped the project root.");
        const auto target = fs::weakly_canonical(
            lease_.ProjectRoot() / candidate.relativeTarget, error);
        if (error || !Within(lease_.ProjectRoot(), target) || !fs::is_regular_file(target))
            return FailBeforeCommit("Project.Mutation.InvalidTarget",
                "Every canonical target must be an existing regular file inside the project.");
        const auto oldBytes = ReadBytes(target, error);
        if (error) return FailBeforeCommit(
            "Project.Mutation.TargetReadFailed", error.message());
        const auto staged = transactionRoot_ / "staged" /
            (std::to_string(index++) + "-" + target.filename().string());
        const auto publication = DefaultDocumentPersistence()->Publish(
            staged, candidate.bytes, journal_.transactionId);
        if (!publication.renamed || publication.error)
            return FailBeforeCommit(
                "Project.Mutation.StageWriteFailed", publication.error.message());
#ifndef _WIN32
        struct stat targetState {}, stagedState {};
        if (::stat(target.c_str(), &targetState) != 0 ||
            ::stat(staged.c_str(), &stagedState) != 0 ||
            targetState.st_dev != stagedState.st_dev)
            return FailBeforeCommit("Project.Mutation.CrossFilesystemStaging",
                "Staged and canonical files must share a filesystem.");
#endif
        journal_.files.push_back({candidate.relativeTarget.lexically_normal(),
            fs::relative(staged, lease_.ProjectRoot()), Hash(oldBytes),
            Hash(candidate.bytes), Identity(target, error), Identity(staged, error), false});
        if (error) return FailBeforeCommit(
            "Project.Mutation.IdentityFailed", error.message());
    }
    journal_.phase = ProjectMutationPhase::Staging;
    auto result = Persist();
    if (!result.succeeded)
        return FailBeforeCommit(result.code, result.message);
    if (injector_ && injector_(ProjectMutationFaultPoint::AfterStaging))
        return Fail("Project.Mutation.InjectedCrash", "Crash injected after staging.");
    if (injector_ && injector_(ProjectMutationFaultPoint::AfterDurableJournal))
        return Fail("Project.Mutation.InjectedCrash", "Crash injected after durable journal.");
    return Ok();
}

ProjectMutationResult ProjectMutationTransaction::MarkCommit()
{
    if (journal_.phase != ProjectMutationPhase::Staging)
        return Fail("Project.Mutation.InvalidState", "Commit marking requires staged files.");
    if (const auto reviewed = RevalidateReviewedInventory(false); !reviewed.succeeded)
        return FailBeforeCommit(reviewed.code, reviewed.message);
    journal_.commitMarker = true;
    journal_.phase = ProjectMutationPhase::CommitMarked;
    auto result = Persist();
    if (!result.succeeded)
    {
        journal_.commitMarker = false;
        return FailBeforeCommit(result.code, result.message);
    }
    if (injector_ && injector_(ProjectMutationFaultPoint::AfterCommitMarker))
        return Fail("Project.Mutation.InjectedCrash", "Crash injected after commit marker.");
    return Ok();
}

ProjectMutationResult ProjectMutationTransaction::Publish()
{
    if (!journal_.commitMarker || journal_.phase != ProjectMutationPhase::CommitMarked)
        return Fail("Project.Mutation.InvalidState", "Publication requires a durable commit marker.");
    if (!lease_.Revalidate())
        return EnterRecoveryRequired("Project.Mutation.LeaseLost",
            "The writer lease is no longer valid.");
    std::error_code error;
    if (const auto reviewed = RevalidateReviewedInventory(false); !reviewed.succeeded)
        return EnterRecoveryRequired(reviewed.code, reviewed.message);
    journal_.phase = ProjectMutationPhase::Publishing;
    if (auto persisted = Persist(); !persisted.succeeded)
        return EnterRecoveryRequired(persisted.code, persisted.message);
    for (std::size_t index = 0; index < journal_.files.size(); ++index)
    {
        auto& file = journal_.files[index];
        if (const auto reviewed = RevalidateReviewedInventory(true); !reviewed.succeeded)
            return EnterRecoveryRequired(reviewed.code, reviewed.message);
        const auto staged = lease_.ProjectRoot() / file.stagedPath;
        const auto bytes = ReadBytes(staged, error);
        if (error) return EnterRecoveryRequired(
            "Project.Mutation.StageReadFailed", error.message());
        const auto publication = DefaultDocumentPersistence()->Publish(
            lease_.ProjectRoot() / file.relativeTarget, bytes, journal_.transactionId);
        if (!publication.renamed || publication.error)
            return EnterRecoveryRequired(
                "Project.Mutation.PublishFailed", publication.error.message());
        file.published = true;
        if (auto persisted = Persist(); !persisted.succeeded)
            return EnterRecoveryRequired(persisted.code, persisted.message);
        const auto point = index == 0 ? ProjectMutationFaultPoint::AfterFirstPublication
                                     : ProjectMutationFaultPoint::AfterSecondPublication;
        if (injector_ && injector_(point))
            return Fail("Project.Mutation.InjectedCrash", "Crash injected during publication.");
    }
    if (injector_ && injector_(ProjectMutationFaultPoint::BeforeFinalVerification))
        return Fail("Project.Mutation.InjectedCrash", "Crash injected before final verification.");
    for (const auto& file : journal_.files)
        if (HashFile(lease_.ProjectRoot() / file.relativeTarget, error) != file.newHash || error)
            return EnterRecoveryRequired("Project.Mutation.FinalVerificationFailed",
                "Published canonical bytes did not match the reviewed candidate.");
    journal_.phase = ProjectMutationPhase::Committed;
    if (auto persisted = Persist(); !persisted.succeeded) return persisted;
    fs::remove_all(transactionRoot_, error);
    return error ? Fail("Project.Mutation.CleanupFailed", error.message()) : Ok();
}

ProjectMutationResult ProjectMutationTransaction::Cancel()
{
    if (journal_.commitMarker)
        return Fail("Project.Mutation.CannotCancelCommitted",
            "A commit-marked transaction must publish or recover.");
    std::error_code error;
    fs::remove_all(transactionRoot_, error);
    journal_.phase = ProjectMutationPhase::Cancelled;
    return error ? Fail("Project.Mutation.CleanupFailed", error.message()) : Ok();
}
const ProjectMutationJournal& ProjectMutationTransaction::Journal() const noexcept
{ return journal_; }
const fs::path& ProjectMutationTransaction::JournalPath() const noexcept
{ return journalPath_; }

std::vector<ProjectMutationRecoveryResult> ProjectMutationRecovery::Scan(
    const fs::path& projectRoot)
{
    std::vector<ProjectMutationRecoveryResult> results;
    std::error_code error;
    const auto root = fs::canonical(projectRoot, error);
    if (error) return {{ProjectMutationRecoveryAction::RecoveryRequired, {},
        "Project.Mutation.InvalidRoot", error.message()}};
    const auto transactions = CoordinationRoot(root);
    if (!fs::exists(transactions, error)) return results;
    std::vector<fs::path> journals;
    for (const auto& entry : fs::directory_iterator(transactions, error))
        if (entry.is_directory() && fs::is_regular_file(entry.path() / "journal.v1.json"))
            journals.push_back(entry.path() / "journal.v1.json");
    std::sort(journals.begin(), journals.end());
    for (const auto& journal : journals) results.push_back(Recover(root, journal));
    return results;
}

ProjectMutationRecoveryResult ProjectMutationRecovery::Recover(
    const fs::path& projectRoot, const fs::path& journalPath)
{
    auto [lease, leaseResult] = ProjectMutationLease::Acquire(
        projectRoot, "package-recovery");
    if (!leaseResult.succeeded)
        return {ProjectMutationRecoveryAction::RecoveryRequired, journalPath,
            leaseResult.code, leaseResult.message};
    auto [loaded, loadResult] = ProjectMutationJournal::Load(journalPath);
    if (!loaded)
        return {loadResult.code == "Project.Mutation.UnsupportedJournal"
                ? ProjectMutationRecoveryAction::UnsupportedJournal
                : ProjectMutationRecoveryAction::RecoveryRequired,
            journalPath, loadResult.code, loadResult.message};
    auto journal = *loaded;
    std::error_code error;
    const auto root = fs::canonical(projectRoot, error);
    if (error || Hash(root.generic_string()) != journal.projectIdentity)
        return {ProjectMutationRecoveryAction::RecoveryRequired, journalPath,
            "Project.Mutation.ProjectIdentityMismatch",
            "The journal does not belong to this canonical project."};

    bool allOld = true;
    bool allNew = true;
    for (const auto& file : journal.files)
    {
        const auto target = root / file.relativeTarget;
        const auto hash = HashFile(target, error);
        if (error || (hash != file.oldHash && hash != file.newHash))
            return {ProjectMutationRecoveryAction::RecoveryRequired, journalPath,
                "Project.Mutation.RecoveryDivergence",
                "Canonical bytes are outside the journal old/new identity set."};
        allOld = allOld && hash == file.oldHash;
        allNew = allNew && hash == file.newHash;
    }
    if (!journal.commitMarker)
    {
        if (!allOld)
            return {ProjectMutationRecoveryAction::RecoveryRequired, journalPath,
                "Project.Mutation.UnmarkedCanonicalChange",
                "An uncommitted transaction changed canonical bytes."};
        fs::remove_all(journalPath.parent_path(), error);
        return {error ? ProjectMutationRecoveryAction::RecoveryRequired
                      : ProjectMutationRecoveryAction::Discarded,
            journalPath, error ? "Project.Mutation.CleanupFailed" : std::string{},
            error.message()};
    }
    for (const auto& file : journal.files)
    {
        const auto target = root / file.relativeTarget;
        if (HashFile(target, error) == file.newHash && !error) continue;
        const auto staged = root / file.stagedPath;
        const auto bytes = ReadBytes(staged, error);
        if (error || Hash(bytes) != file.newHash)
            return {ProjectMutationRecoveryAction::RecoveryRequired, journalPath,
                "Project.Mutation.StagedRecoveryInvalid",
                "The committed staged candidate is missing or changed."};
        const auto publication = DefaultDocumentPersistence()->Publish(
            target, bytes, journal.transactionId);
        if (!publication.renamed || publication.error)
            return {ProjectMutationRecoveryAction::RecoveryRequired, journalPath,
                "Project.Mutation.RecoveryPublishFailed", publication.error.message()};
    }
    fs::remove_all(journalPath.parent_path(), error);
    return {error ? ProjectMutationRecoveryAction::RecoveryRequired
                  : (allNew ? ProjectMutationRecoveryAction::Finalized
                            : ProjectMutationRecoveryAction::RolledForward),
        journalPath, error ? "Project.Mutation.CleanupFailed" : std::string{},
        error.message()};
}
}
