// SPDX-License-Identifier: MPL-2.0

#pragma once

#include "SagaEditor/Documents/TextDocument.h"

#include <memory>
#include <cstdint>
#include <functional>
#include <map>

namespace SagaEditor
{

class ProjectDocument final : public IDocument
{
public:
    using ChangeSubscription = std::uint64_t;
    using ChangeCallback = std::function<void()>;
    static constexpr ChangeSubscription InvalidChangeSubscription = 0;
    explicit ProjectDocument(std::filesystem::path sourcePath);
    ~ProjectDocument() override;

    [[nodiscard]] const DocumentSnapshot& Snapshot() const noexcept override;
    [[nodiscard]] const std::vector<DocumentDiagnostic>& Diagnostics() const noexcept override;
    [[nodiscard]] const std::string& Text() const noexcept;
    [[nodiscard]] bool ReplaceText(std::string text);
    [[nodiscard]] bool Load() override;
    [[nodiscard]] DocumentSaveStatus Save() override;
    [[nodiscard]] bool Reload() override;
    [[nodiscard]] DocumentExternalChangeState CheckExternalChange() override;
    [[nodiscard]] DocumentCloseStatus RequestClose() const noexcept override;
    [[nodiscard]] bool CanUndo() const noexcept override;
    [[nodiscard]] bool CanRedo() const noexcept override;
    [[nodiscard]] bool Undo() override;
    [[nodiscard]] bool Redo() override;
    [[nodiscard]] ChangeSubscription SubscribeChanges(ChangeCallback callback);
    void UnsubscribeChanges(ChangeSubscription subscription);

private:
    void NotifyChanged();
    std::unique_ptr<TextDocument> text_;
    std::map<ChangeSubscription, ChangeCallback> callbacks_;
    ChangeSubscription nextSubscription_ = 1;
};

class CSharpSourceDocument final : public IDocument
{
public:
    using ChangeSubscription = std::uint64_t;
    using ChangeCallback = std::function<void()>;
    static constexpr ChangeSubscription InvalidChangeSubscription = 0;
    explicit CSharpSourceDocument(std::filesystem::path sourcePath);
    ~CSharpSourceDocument() override;

    [[nodiscard]] const DocumentSnapshot& Snapshot() const noexcept override;
    [[nodiscard]] const std::vector<DocumentDiagnostic>& Diagnostics() const noexcept override;
    [[nodiscard]] const std::string& Text() const noexcept;
    [[nodiscard]] const DocumentRecoverySnapshot& Recovery() const noexcept;
    [[nodiscard]] bool ReplaceText(std::string text);
    [[nodiscard]] bool ReplaceTextProvisional(
        std::string text, const std::function<bool(const CSharpSourceDocument&)>& validateCandidate);
    [[nodiscard]] bool Load() override;
    [[nodiscard]] DocumentSaveStatus Save() override;
    [[nodiscard]] bool Reload() override;
    [[nodiscard]] DocumentExternalChangeState CheckExternalChange() override;
    [[nodiscard]] DocumentCloseStatus RequestClose() const noexcept override;
    [[nodiscard]] bool CanUndo() const noexcept override;
    [[nodiscard]] bool CanRedo() const noexcept override;
    [[nodiscard]] bool Undo() override;
    [[nodiscard]] bool Redo() override;
    [[nodiscard]] bool RecoverAsDirtyBuffer();
    [[nodiscard]] bool DiscardRecovery();
    [[nodiscard]] ChangeSubscription SubscribeChanges(ChangeCallback callback);
    void UnsubscribeChanges(ChangeSubscription subscription);

private:
    void NotifyChanged();
    std::unique_ptr<TextDocument> text_;
    std::map<ChangeSubscription, ChangeCallback> callbacks_;
    ChangeSubscription nextSubscription_ = 1;
};

class InputMapDocument final : public IDocument
{
public:
    using ChangeSubscription = std::uint64_t;
    using ChangeCallback = std::function<void()>;
    static constexpr ChangeSubscription InvalidChangeSubscription = 0;
    explicit InputMapDocument(std::filesystem::path sourcePath);
    ~InputMapDocument() override;

    [[nodiscard]] const DocumentSnapshot& Snapshot() const noexcept override;
    [[nodiscard]] const std::vector<DocumentDiagnostic>& Diagnostics() const noexcept override;
    [[nodiscard]] const std::string& Text() const noexcept;
    [[nodiscard]] bool ReplaceText(std::string text);
    [[nodiscard]] bool Load() override;
    [[nodiscard]] DocumentSaveStatus Save() override;
    [[nodiscard]] bool Reload() override;
    [[nodiscard]] DocumentExternalChangeState CheckExternalChange() override;
    [[nodiscard]] DocumentCloseStatus RequestClose() const noexcept override;
    [[nodiscard]] bool CanUndo() const noexcept override;
    [[nodiscard]] bool CanRedo() const noexcept override;
    [[nodiscard]] bool Undo() override;
    [[nodiscard]] bool Redo() override;
    [[nodiscard]] ChangeSubscription SubscribeChanges(ChangeCallback callback);
    void UnsubscribeChanges(ChangeSubscription subscription);

private:
    void NotifyChanged();
    std::unique_ptr<TextDocument> text_;
    std::map<ChangeSubscription, ChangeCallback> callbacks_;
    ChangeSubscription nextSubscription_ = 1;
};

} // namespace SagaEditor
