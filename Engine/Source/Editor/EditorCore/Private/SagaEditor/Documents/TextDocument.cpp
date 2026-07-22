// SPDX-License-Identifier: MPL-2.0

#include "SagaEditor/Documents/TextDocument.h"

#include <nlohmann/json.hpp>

#include <fstream>
#include <utility>

namespace SagaEditor
{
namespace
{
std::filesystem::path FindProjectRoot(const std::filesystem::path& source)
{
    auto cursor = source.parent_path();
    std::error_code error;
    while (!cursor.empty())
    {
        for (std::filesystem::directory_iterator it(cursor, error), end; !error && it != end; it.increment(error))
            if (it->is_regular_file(error) && it->path().extension() == ".sagaproj") return cursor;
        error.clear();
        const auto parent = cursor.parent_path();
        if (parent == cursor) break;
        cursor = parent;
    }
    return source.parent_path();
}

std::string ProjectIdentity(const std::filesystem::path& projectRoot)
{
    std::error_code error;
    for (std::filesystem::directory_iterator it(projectRoot, error), end; !error && it != end; it.increment(error))
    {
        if (!it->is_regular_file(error) || it->path().extension() != ".sagaproj") continue;
        const auto bytes = ReadDocumentBytes(it->path(), error);
        if (!error) return HashDocumentBytes(bytes);
    }
    return HashDocumentBytes(projectRoot.generic_string());
}

std::string SafeDocumentId(std::string_view documentId)
{
    std::string safe;
    for (const auto character : documentId)
        if ((character >= 'a' && character <= 'z') ||
            (character >= 'A' && character <= 'Z') ||
            (character >= '0' && character <= '9') || character == '-' || character == '_')
            safe.push_back(character);
    return safe.empty() ? "document" : safe;
}
}

TextDocument::TextDocument(std::string documentId,
                           DocumentKind kind,
                           std::filesystem::path sourcePath,
                           std::uint32_t schemaVersion,
                           bool canSave,
                           TextDocumentValidator validator,
                           std::shared_ptr<IDocumentPersistence> persistence)
    : validator_(std::move(validator)),
      persistence_(persistence ? std::move(persistence) : DefaultDocumentPersistence())
{
    snapshot_.documentId = std::move(documentId);
    snapshot_.kind = kind;
    std::error_code error;
    snapshot_.canonicalSourcePath = std::filesystem::weakly_canonical(sourcePath, error);
    if (error)
        snapshot_.canonicalSourcePath = std::filesystem::absolute(sourcePath).lexically_normal();
    snapshot_.schemaVersion = schemaVersion;
    snapshot_.canSave = canSave;
    const auto projectRoot = FindProjectRoot(snapshot_.canonicalSourcePath);
    const auto pathIdentity = HashDocumentBytes(
        snapshot_.canonicalSourcePath.generic_string()).substr(0, 16);
    snapshot_.recoveryReference = projectRoot / ".saga" / "user" / "recovery" /
        (SafeDocumentId(snapshot_.documentId) + "-" + pathIdentity + ".json");
    recovery_.projectIdentity = ProjectIdentity(projectRoot);
}

const DocumentSnapshot& TextDocument::Snapshot() const noexcept { return snapshot_; }
const std::vector<DocumentDiagnostic>& TextDocument::Diagnostics() const noexcept { return diagnostics_; }
const std::string& TextDocument::Text() const noexcept { return text_; }
const DocumentRecoverySnapshot& TextDocument::Recovery() const noexcept { return recovery_; }

bool TextDocument::Validate(const std::string& text)
{
    diagnostics_ = validator_ ? validator_(text) : std::vector<DocumentDiagnostic>{};
    snapshot_.validation = diagnostics_.empty() ?
        DocumentValidationState::Valid : DocumentValidationState::Invalid;
    return diagnostics_.empty();
}

bool TextDocument::Load()
{
    std::error_code error;
    auto bytes = ReadDocumentBytes(snapshot_.canonicalSourcePath, error);
    if (error)
    {
        diagnostics_ = {{"Document.ReadFailed", error.message(), snapshot_.canonicalSourcePath}};
        snapshot_.validation = DocumentValidationState::Invalid;
        return false;
    }
    if (!Validate(bytes))
        return false;
    text_ = std::move(bytes);
    snapshot_.baseDiskHash = HashDocumentBytes(text_);
    snapshot_.dirty = false;
    snapshot_.externalChange = DocumentExternalChangeState::Unchanged;
    snapshot_.inMemoryRevision = 1;
    history_ = {text_};
    historyCursor_ = 1;
    (void)InspectRecovery();
    return true;
}

void TextDocument::Commit(std::string text, bool recordUndo)
{
    text_ = std::move(text);
    ++snapshot_.inMemoryRevision;
    snapshot_.dirty = HashDocumentBytes(text_) != snapshot_.baseDiskHash;
    if (!recordUndo)
        return;
    history_.resize(historyCursor_);
    history_.push_back(text_);
    historyCursor_ = history_.size();
}

bool TextDocument::ReplaceText(std::string text)
{
    if (!Validate(text))
        return false;
    Commit(std::move(text), true);
    PublishRecovery();
    return true;
}

DocumentSaveStatus TextDocument::Save()
{
    if (!snapshot_.canSave)
        return DocumentSaveStatus::WriteFailed;
    if (!snapshot_.dirty)
        return DocumentSaveStatus::NotDirty;
    if (!Validate(text_))
        return DocumentSaveStatus::ValidationFailed;
    if (CheckExternalChange() != DocumentExternalChangeState::Unchanged)
        return DocumentSaveStatus::Conflict;
    const auto publication = persistence_->Publish(snapshot_.canonicalSourcePath, text_,
        std::to_string(snapshot_.inMemoryRevision));
    if (!publication.renamed)
    {
        diagnostics_ = {{"Document.AtomicSaveFailed", publication.error.message(), snapshot_.canonicalSourcePath}};
        return DocumentSaveStatus::WriteFailed;
    }
    snapshot_.baseDiskHash = HashDocumentBytes(text_);
    snapshot_.dirty = false;
    snapshot_.externalChange = DocumentExternalChangeState::Unchanged;
    diagnostics_.clear();
    if (publication.error)
        diagnostics_.push_back({"Document.PostRenameDurabilityWarning", publication.error.message(),
            snapshot_.canonicalSourcePath});
    std::error_code cleanupError;
    if (!persistence_->Remove(snapshot_.recoveryReference, cleanupError) && cleanupError)
        diagnostics_.push_back({"Document.RecoveryCleanupDeferred", cleanupError.message(),
            snapshot_.recoveryReference});
    const auto projectIdentity = recovery_.projectIdentity;
    recovery_ = {};
    recovery_.projectIdentity = projectIdentity;
    return DocumentSaveStatus::Saved;
}

bool TextDocument::Reload() { return Load(); }

DocumentExternalChangeState TextDocument::CheckExternalChange()
{
    std::error_code error;
    const auto bytes = ReadDocumentBytes(snapshot_.canonicalSourcePath, error);
    if (error)
        snapshot_.externalChange = DocumentExternalChangeState::Missing;
    else
        snapshot_.externalChange = HashDocumentBytes(bytes) == snapshot_.baseDiskHash ?
            DocumentExternalChangeState::Unchanged : DocumentExternalChangeState::Changed;
    return snapshot_.externalChange;
}

DocumentCloseStatus TextDocument::RequestClose() const noexcept
{
    return snapshot_.dirty ? DocumentCloseStatus::RequiresDecision : DocumentCloseStatus::Closed;
}

bool TextDocument::CanUndo() const noexcept { return historyCursor_ > 1; }
bool TextDocument::CanRedo() const noexcept { return historyCursor_ < history_.size(); }

bool TextDocument::Undo()
{
    if (!CanUndo()) return false;
    --historyCursor_;
    (void)Validate(history_[historyCursor_ - 1]);
    Commit(history_[historyCursor_ - 1], false);
    PublishRecovery();
    return true;
}

bool TextDocument::Redo()
{
    if (!CanRedo()) return false;
    (void)Validate(history_[historyCursor_]);
    Commit(history_[historyCursor_], false);
    ++historyCursor_;
    PublishRecovery();
    return true;
}

void TextDocument::PublishRecovery()
{
    if (!snapshot_.dirty)
    {
        std::error_code ignored;
        (void)persistence_->Remove(snapshot_.recoveryReference, ignored);
        return;
    }
    const nlohmann::json journal = {
        {"baseDiskHash", snapshot_.baseDiskHash},
        {"candidateHash", HashDocumentBytes(text_)},
        {"canonicalPath", snapshot_.canonicalSourcePath.generic_string()},
        {"documentId", snapshot_.documentId},
        {"inMemoryRevision", snapshot_.inMemoryRevision},
        {"projectIdentity", recovery_.projectIdentity},
        {"recoverableText", text_},
        {"schemaVersion", 1},
    };
    const auto result = persistence_->Publish(snapshot_.recoveryReference,
        journal.dump(2) + "\n", "recovery-" + std::to_string(snapshot_.inMemoryRevision));
    if (!result.renamed)
    {
        recovery_.status = DocumentRecoveryStatus::IoFailure;
        recovery_.diagnostic = {"Document.RecoveryPublishFailed", result.error.message(),
            snapshot_.recoveryReference};
        return;
    }
    recovery_.status = DocumentRecoveryStatus::Recoverable;
    recovery_.documentId = snapshot_.documentId;
    recovery_.canonicalPath = snapshot_.canonicalSourcePath;
    recovery_.baseDiskHash = snapshot_.baseDiskHash;
    recovery_.candidateHash = HashDocumentBytes(text_);
    recovery_.inMemoryRevision = snapshot_.inMemoryRevision;
    recovery_.recoverableText = text_;
}

DocumentRecoveryStatus TextDocument::InspectRecovery()
{
    const auto projectIdentity = recovery_.projectIdentity;
    recovery_ = {};
    recovery_.projectIdentity = projectIdentity;
    std::error_code error;
    if (!std::filesystem::exists(snapshot_.recoveryReference, error))
    {
        if (error)
        {
            recovery_.status = DocumentRecoveryStatus::IoFailure;
            recovery_.diagnostic = {"Document.RecoveryReadFailed", error.message(), snapshot_.recoveryReference};
        }
        return recovery_.status;
    }
    try
    {
        const auto bytes = ReadDocumentBytes(snapshot_.recoveryReference, error);
        if (error) throw std::system_error(error);
        const auto journal = nlohmann::json::parse(bytes);
        recovery_.documentId = journal.value("documentId", std::string{});
        recovery_.canonicalPath = journal.value("canonicalPath", std::string{});
        recovery_.baseDiskHash = journal.value("baseDiskHash", std::string{});
        recovery_.candidateHash = journal.value("candidateHash", std::string{});
        recovery_.inMemoryRevision = journal.value("inMemoryRevision", 0ull);
        recovery_.recoverableText = journal.value("recoverableText", std::string{});
        const auto journalProject = journal.value("projectIdentity", std::string{});
        const auto candidateValid = recovery_.candidateHash == HashDocumentBytes(recovery_.recoverableText);
        if (journal.value("schemaVersion", -1) != 1 ||
            recovery_.documentId != snapshot_.documentId ||
            recovery_.canonicalPath != snapshot_.canonicalSourcePath ||
            journalProject != projectIdentity || !candidateValid)
        {
            recovery_.status = DocumentRecoveryStatus::Invalid;
            recovery_.diagnostic = {"Document.RecoveryInvalid",
                "Recovery identity or candidate hash is invalid.", snapshot_.recoveryReference};
            return recovery_.status;
        }
        if (recovery_.candidateHash == snapshot_.baseDiskHash)
        {
            recovery_.status = DocumentRecoveryStatus::Obsolete;
            recovery_.diagnostic = {"Document.RecoveryObsolete",
                "Recovery candidate already matches the canonical file.", snapshot_.recoveryReference};
            return recovery_.status;
        }
        if (recovery_.baseDiskHash != snapshot_.baseDiskHash)
        {
            recovery_.status = DocumentRecoveryStatus::StaleBase;
            recovery_.diagnostic = {"Document.RecoveryStaleBase",
                "Canonical source changed after the recovery candidate was captured.", snapshot_.recoveryReference};
            return recovery_.status;
        }
        recovery_.status = DocumentRecoveryStatus::Recoverable;
    }
    catch (const std::exception& exception)
    {
        recovery_.status = DocumentRecoveryStatus::Invalid;
        recovery_.diagnostic = {"Document.RecoveryInvalid", exception.what(), snapshot_.recoveryReference};
    }
    return recovery_.status;
}

bool TextDocument::RecoverAsDirtyBuffer()
{
    if (InspectRecovery() != DocumentRecoveryStatus::Recoverable ||
        !Validate(recovery_.recoverableText)) return false;
    const auto recovered = recovery_.recoverableText;
    Commit(recovered, true);
    PublishRecovery();
    return snapshot_.dirty;
}

bool TextDocument::DiscardRecovery()
{
    std::error_code error;
    const auto removed = persistence_->Remove(snapshot_.recoveryReference, error);
    if (!removed && error)
    {
        recovery_.status = DocumentRecoveryStatus::IoFailure;
        recovery_.diagnostic = {"Document.RecoveryDiscardFailed", error.message(),
            snapshot_.recoveryReference};
        return false;
    }
    const auto projectIdentity = recovery_.projectIdentity;
    recovery_ = {};
    recovery_.projectIdentity = projectIdentity;
    return true;
}

} // namespace SagaEditor
