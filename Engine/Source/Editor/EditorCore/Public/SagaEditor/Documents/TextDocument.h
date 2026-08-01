// SPDX-License-Identifier: MPL-2.0

#pragma once

#include "SagaEditor/Documents/Document.h"
#include "SagaEditor/Documents/DocumentPersistence.h"

#include <functional>

namespace SagaEditor
{

using TextDocumentValidator =
    std::function<std::vector<DocumentDiagnostic>(const std::string&)>;

enum class DocumentRecoveryStatus
{
    None,
    Recoverable,
    StaleBase,
    Invalid,
    Obsolete,
    IoFailure,
};

struct DocumentRecoverySnapshot final
{
    DocumentRecoveryStatus status = DocumentRecoveryStatus::None;
    std::string projectIdentity;
    std::string documentId;
    std::filesystem::path canonicalPath;
    std::string baseDiskHash;
    std::string candidateHash;
    std::uint64_t inMemoryRevision = 0;
    std::string recoverableText;
    DocumentDiagnostic diagnostic;
};

class TextDocument final : public IDocument
{
public:
    TextDocument(std::string documentId,
                 DocumentKind kind,
                 std::filesystem::path sourcePath,
                 std::uint32_t schemaVersion,
                 bool canSave,
                 TextDocumentValidator validator = {},
                 std::shared_ptr<IDocumentPersistence> persistence = {});

    [[nodiscard]] const DocumentSnapshot& Snapshot() const noexcept override;
    [[nodiscard]] const std::vector<DocumentDiagnostic>& Diagnostics() const noexcept override;
    [[nodiscard]] const std::string& Text() const noexcept;
    [[nodiscard]] const DocumentRecoverySnapshot& Recovery() const noexcept;

    [[nodiscard]] bool Load() override;
    [[nodiscard]] bool ReplaceText(std::string text);
    [[nodiscard]] bool ReplaceTextProvisional(
        std::string text, const std::function<bool()>& validateCandidate);
    [[nodiscard]] DocumentSaveStatus Save() override;
    [[nodiscard]] bool Reload() override;
    [[nodiscard]] DocumentExternalChangeState CheckExternalChange() override;
    [[nodiscard]] DocumentCloseStatus RequestClose() const noexcept override;
    [[nodiscard]] bool CanUndo() const noexcept override;
    [[nodiscard]] bool CanRedo() const noexcept override;
    [[nodiscard]] bool Undo() override;
    [[nodiscard]] bool Redo() override;
    [[nodiscard]] DocumentRecoveryStatus InspectRecovery();
    [[nodiscard]] bool RecoverAsDirtyBuffer();
    [[nodiscard]] bool DiscardRecovery();

private:
    [[nodiscard]] bool Validate(const std::string& text);
    void Commit(std::string text, bool recordUndo);
    void PublishRecovery();

    DocumentSnapshot snapshot_;
    TextDocumentValidator validator_;
    std::shared_ptr<IDocumentPersistence> persistence_;
    DocumentRecoverySnapshot recovery_;
    std::vector<DocumentDiagnostic> diagnostics_;
    std::string text_;
    std::vector<std::string> history_;
    std::size_t historyCursor_ = 0;
};

} // namespace SagaEditor
