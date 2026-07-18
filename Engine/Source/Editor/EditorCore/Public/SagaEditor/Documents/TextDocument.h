// SPDX-FileCopyrightText: 2026 Arda Koyuncu
// SPDX-License-Identifier: MPL-2.0

#pragma once

#include "SagaEditor/Documents/Document.h"

#include <functional>

namespace SagaEditor
{

using TextDocumentValidator =
    std::function<std::vector<DocumentDiagnostic>(const std::string&)>;

class TextDocument final : public IDocument
{
public:
    TextDocument(std::string documentId,
                 DocumentKind kind,
                 std::filesystem::path sourcePath,
                 std::uint32_t schemaVersion,
                 bool canSave,
                 TextDocumentValidator validator = {});

    [[nodiscard]] const DocumentSnapshot& Snapshot() const noexcept override;
    [[nodiscard]] const std::vector<DocumentDiagnostic>& Diagnostics() const noexcept override;
    [[nodiscard]] const std::string& Text() const noexcept;

    [[nodiscard]] bool Load() override;
    [[nodiscard]] bool ReplaceText(std::string text);
    [[nodiscard]] DocumentSaveStatus Save() override;
    [[nodiscard]] bool Reload() override;
    [[nodiscard]] DocumentExternalChangeState CheckExternalChange() override;
    [[nodiscard]] DocumentCloseStatus RequestClose() const noexcept override;
    [[nodiscard]] bool CanUndo() const noexcept override;
    [[nodiscard]] bool CanRedo() const noexcept override;
    [[nodiscard]] bool Undo() override;
    [[nodiscard]] bool Redo() override;

private:
    [[nodiscard]] bool Validate(const std::string& text);
    void Commit(std::string text, bool recordUndo);

    DocumentSnapshot snapshot_;
    TextDocumentValidator validator_;
    std::vector<DocumentDiagnostic> diagnostics_;
    std::string text_;
    std::vector<std::string> history_;
    std::size_t historyCursor_ = 0;
};

} // namespace SagaEditor
