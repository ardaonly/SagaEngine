// SPDX-FileCopyrightText: 2026 Arda Koyuncu
// SPDX-License-Identifier: MPL-2.0

#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace SagaEditor
{

enum class DocumentKind : std::uint8_t
{
    Project,
    Scene,
    CSharpSource,
    InputMap,
};

enum class DocumentValidationState : std::uint8_t
{
    Unknown,
    Valid,
    Invalid,
};

enum class DocumentExternalChangeState : std::uint8_t
{
    Unchanged,
    Changed,
    Missing,
};

enum class DocumentSaveStatus : std::uint8_t
{
    Saved,
    NotDirty,
    Conflict,
    ValidationFailed,
    WriteFailed,
};

enum class DocumentCloseStatus : std::uint8_t
{
    Closed,
    RequiresDecision,
};

struct DocumentDiagnostic
{
    std::string code;
    std::string message;
    std::filesystem::path path;
    std::uint32_t line = 0;
    std::uint32_t column = 0;
};

struct DocumentSnapshot
{
    std::string documentId;
    DocumentKind kind = DocumentKind::Project;
    std::filesystem::path canonicalSourcePath;
    std::uint32_t schemaVersion = 0;
    std::string baseDiskHash;
    std::uint64_t inMemoryRevision = 0;
    bool dirty = false;
    DocumentValidationState validation = DocumentValidationState::Unknown;
    bool canSave = false;
    DocumentExternalChangeState externalChange = DocumentExternalChangeState::Unchanged;
    std::filesystem::path recoveryReference;
};

[[nodiscard]] std::string HashDocumentBytes(const std::string& bytes);
[[nodiscard]] std::string ReadDocumentBytes(const std::filesystem::path& path,
                                            std::error_code& error);
[[nodiscard]] bool AtomicWriteDocument(const std::filesystem::path& path,
                                       const std::string& bytes,
                                       const std::string& operationId,
                                       std::error_code& error);

class IDocument
{
public:
    virtual ~IDocument() = default;
    [[nodiscard]] virtual const DocumentSnapshot& Snapshot() const noexcept = 0;
    [[nodiscard]] virtual const std::vector<DocumentDiagnostic>& Diagnostics() const noexcept = 0;
    [[nodiscard]] virtual bool Load() = 0;
    [[nodiscard]] virtual DocumentSaveStatus Save() = 0;
    [[nodiscard]] virtual bool Reload() = 0;
    [[nodiscard]] virtual DocumentExternalChangeState CheckExternalChange() = 0;
    [[nodiscard]] virtual DocumentCloseStatus RequestClose() const noexcept = 0;
    [[nodiscard]] virtual bool CanUndo() const noexcept = 0;
    [[nodiscard]] virtual bool CanRedo() const noexcept = 0;
    [[nodiscard]] virtual bool Undo() = 0;
    [[nodiscard]] virtual bool Redo() = 0;
};

} // namespace SagaEditor
