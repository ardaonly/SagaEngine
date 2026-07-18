// SPDX-FileCopyrightText: 2026 Arda Koyuncu
// SPDX-License-Identifier: MPL-2.0

#include "SagaEditor/Documents/TextDocument.h"

#include <utility>

namespace SagaEditor
{

TextDocument::TextDocument(std::string documentId,
                           DocumentKind kind,
                           std::filesystem::path sourcePath,
                           std::uint32_t schemaVersion,
                           bool canSave,
                           TextDocumentValidator validator)
    : validator_(std::move(validator))
{
    snapshot_.documentId = std::move(documentId);
    snapshot_.kind = kind;
    std::error_code error;
    snapshot_.canonicalSourcePath = std::filesystem::weakly_canonical(sourcePath, error);
    if (error)
        snapshot_.canonicalSourcePath = std::filesystem::absolute(sourcePath).lexically_normal();
    snapshot_.schemaVersion = schemaVersion;
    snapshot_.canSave = canSave;
    snapshot_.recoveryReference = snapshot_.canonicalSourcePath.parent_path() /
        ".saga" / "recovery" / (snapshot_.documentId + ".json");
}

const DocumentSnapshot& TextDocument::Snapshot() const noexcept { return snapshot_; }
const std::vector<DocumentDiagnostic>& TextDocument::Diagnostics() const noexcept { return diagnostics_; }
const std::string& TextDocument::Text() const noexcept { return text_; }

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
    std::error_code error;
    if (!AtomicWriteDocument(snapshot_.canonicalSourcePath, text_,
                             std::to_string(snapshot_.inMemoryRevision), error))
    {
        diagnostics_ = {{"Document.AtomicSaveFailed", error.message(), snapshot_.canonicalSourcePath}};
        return DocumentSaveStatus::WriteFailed;
    }
    snapshot_.baseDiskHash = HashDocumentBytes(text_);
    snapshot_.dirty = false;
    snapshot_.externalChange = DocumentExternalChangeState::Unchanged;
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
    return true;
}

bool TextDocument::Redo()
{
    if (!CanRedo()) return false;
    (void)Validate(history_[historyCursor_]);
    Commit(history_[historyCursor_], false);
    ++historyCursor_;
    return true;
}

} // namespace SagaEditor
