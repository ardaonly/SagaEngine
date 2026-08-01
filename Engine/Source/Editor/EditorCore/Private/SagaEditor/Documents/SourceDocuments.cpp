// SPDX-License-Identifier: MPL-2.0

#include "SagaEditor/Documents/SourceDocuments.h"

#include <nlohmann/json.hpp>

#include <set>

namespace SagaEditor
{
namespace
{
std::vector<DocumentDiagnostic> ValidateCSharp(const std::string& text)
{
    if (text.find('\0') != std::string::npos)
        return {{"Script.Source.NulByte", "C# source cannot contain NUL bytes."}};
    return {};
}

std::vector<DocumentDiagnostic> ValidateInputMap(const std::string& text)
{
    using Json = nlohmann::json;
    Json root;
    try { root = Json::parse(text); }
    catch (const Json::exception& error) { return {{"Input.JsonInvalid", error.what()}}; }
    if (!root.is_object() || root.value("schemaVersion", -1) != 1 ||
        !root.contains("inputMapId") || !root["inputMapId"].is_string() ||
        root["inputMapId"].get_ref<const std::string&>().empty() ||
        !root.contains("actions") || !root["actions"].is_array() || root.size() != 3)
        return {{"Input.SchemaInvalid", "Input source must use schema version 1 with inputMapId and actions."}};
    std::set<std::string> actionIds;
    std::set<std::pair<std::string, std::string>> physicalBindings;
    for (const auto& action : root["actions"])
    {
        if (!action.is_object() || action.size() != 4 ||
            !action.contains("actionId") || !action["actionId"].is_string() ||
            !action.contains("displayName") || !action["displayName"].is_string() ||
            !action.contains("runtimeRole") || !action["runtimeRole"].is_string() ||
            !action.contains("bindings") || !action["bindings"].is_array())
            return {{"Input.ActionInvalid", "Input actions require actionId, displayName, runtimeRole, and bindings."}};
        const auto actionId = action["actionId"].get<std::string>();
        if (actionId.empty() || !actionIds.insert(actionId).second)
            return {{"Input.ActionDuplicate", "Input action identities must be non-empty and unique."}};
        if (action["displayName"].get_ref<const std::string&>().empty() ||
            action["runtimeRole"].get_ref<const std::string&>().empty())
            return {{"Input.ActionInvalid", "Input action display name and runtime role are required."}};
        for (const auto& binding : action["bindings"])
        {
            if (!binding.is_object() || binding.size() != 3 ||
                !binding.contains("device") || !binding["device"].is_string() ||
                !binding.contains("control") || !binding["control"].is_string() ||
                !binding.contains("displayName") || !binding["displayName"].is_string())
                return {{"Input.BindingInvalid", "Physical bindings require device, control, and displayName."}};
            const auto device = binding["device"].get<std::string>();
            const auto control = binding["control"].get<std::string>();
            if (device.empty() || control.empty() ||
                binding["displayName"].get_ref<const std::string&>().empty())
                return {{"Input.BindingInvalid", "Physical binding values cannot be empty."}};
            if (!physicalBindings.emplace(device, control).second)
                return {{"Input.BindingDuplicate", "A physical binding cannot map ambiguously to multiple actions."}};
        }
    }
    return {};
}

#define SAGA_FORWARD_DOCUMENT(ClassName) \
const DocumentSnapshot& ClassName::Snapshot() const noexcept { return text_->Snapshot(); } \
const std::vector<DocumentDiagnostic>& ClassName::Diagnostics() const noexcept { return text_->Diagnostics(); } \
const std::string& ClassName::Text() const noexcept { return text_->Text(); } \
bool ClassName::Load() { return text_->Load(); } \
DocumentSaveStatus ClassName::Save() { return text_->Save(); } \
bool ClassName::Reload() { return text_->Reload(); } \
DocumentExternalChangeState ClassName::CheckExternalChange() { return text_->CheckExternalChange(); } \
DocumentCloseStatus ClassName::RequestClose() const noexcept { return text_->RequestClose(); } \
bool ClassName::CanUndo() const noexcept { return text_->CanUndo(); } \
bool ClassName::CanRedo() const noexcept { return text_->CanRedo(); } \
bool ClassName::Undo() { return text_->Undo(); } \
bool ClassName::Redo() { return text_->Redo(); }
}

ProjectDocument::ProjectDocument(std::filesystem::path sourcePath)
    : text_(std::make_unique<TextDocument>("project:" + sourcePath.filename().string(),
        DocumentKind::Project, std::move(sourcePath), 0, true)) {}
ProjectDocument::~ProjectDocument() = default;
const DocumentSnapshot& ProjectDocument::Snapshot() const noexcept { return text_->Snapshot(); }
const std::vector<DocumentDiagnostic>& ProjectDocument::Diagnostics() const noexcept { return text_->Diagnostics(); }
const std::string& ProjectDocument::Text() const noexcept { return text_->Text(); }
bool ProjectDocument::ReplaceText(std::string text)
{
    if (!text_->ReplaceText(std::move(text))) return false;
    NotifyChanged(); return true;
}
bool ProjectDocument::Load() { const auto result = text_->Load(); if (result) NotifyChanged(); return result; }
DocumentSaveStatus ProjectDocument::Save()
{ const auto result = text_->Save(); if (result == DocumentSaveStatus::Saved) NotifyChanged(); return result; }
bool ProjectDocument::Reload() { const auto result = text_->Reload(); if (result) NotifyChanged(); return result; }
DocumentExternalChangeState ProjectDocument::CheckExternalChange() { return text_->CheckExternalChange(); }
DocumentCloseStatus ProjectDocument::RequestClose() const noexcept { return text_->RequestClose(); }
bool ProjectDocument::CanUndo() const noexcept { return text_->CanUndo(); }
bool ProjectDocument::CanRedo() const noexcept { return text_->CanRedo(); }
bool ProjectDocument::Undo() { const auto result = text_->Undo(); if (result) NotifyChanged(); return result; }
bool ProjectDocument::Redo() { const auto result = text_->Redo(); if (result) NotifyChanged(); return result; }
ProjectDocument::ChangeSubscription ProjectDocument::SubscribeChanges(ChangeCallback callback)
{
    if (!callback) return InvalidChangeSubscription;
    const auto subscription = nextSubscription_++;
    callbacks_.emplace(subscription, std::move(callback));
    return subscription;
}
void ProjectDocument::UnsubscribeChanges(ChangeSubscription subscription)
{ callbacks_.erase(subscription); }
void ProjectDocument::NotifyChanged()
{
    const auto callbacks = callbacks_;
    for (const auto& [subscription, callback] : callbacks)
    { (void)subscription; if (callback) callback(); }
}

CSharpSourceDocument::CSharpSourceDocument(std::filesystem::path sourcePath)
    : text_(std::make_unique<TextDocument>("script:" + sourcePath.filename().string(),
        DocumentKind::CSharpSource, std::move(sourcePath), 1, true, ValidateCSharp)) {}
CSharpSourceDocument::~CSharpSourceDocument() = default;
const DocumentSnapshot& CSharpSourceDocument::Snapshot() const noexcept { return text_->Snapshot(); }
const std::vector<DocumentDiagnostic>& CSharpSourceDocument::Diagnostics() const noexcept { return text_->Diagnostics(); }
const std::string& CSharpSourceDocument::Text() const noexcept { return text_->Text(); }
const DocumentRecoverySnapshot& CSharpSourceDocument::Recovery() const noexcept { return text_->Recovery(); }
bool CSharpSourceDocument::ReplaceText(std::string text)
{
    if (!text_->ReplaceText(std::move(text))) return false;
    NotifyChanged(); return true;
}
bool CSharpSourceDocument::ReplaceTextProvisional(
    std::string text,
    const std::function<bool(const CSharpSourceDocument&)>& validateCandidate)
{
    const auto result = text_->ReplaceTextProvisional(std::move(text),
        [this, &validateCandidate]
        { return !validateCandidate || validateCandidate(*this); });
    if (result) NotifyChanged();
    return result;
}
bool CSharpSourceDocument::Load() { const auto result = text_->Load(); if (result) NotifyChanged(); return result; }
DocumentSaveStatus CSharpSourceDocument::Save()
{ const auto result = text_->Save(); if (result == DocumentSaveStatus::Saved) NotifyChanged(); return result; }
bool CSharpSourceDocument::Reload() { const auto result = text_->Reload(); if (result) NotifyChanged(); return result; }
DocumentExternalChangeState CSharpSourceDocument::CheckExternalChange() { return text_->CheckExternalChange(); }
DocumentCloseStatus CSharpSourceDocument::RequestClose() const noexcept { return text_->RequestClose(); }
bool CSharpSourceDocument::CanUndo() const noexcept { return text_->CanUndo(); }
bool CSharpSourceDocument::CanRedo() const noexcept { return text_->CanRedo(); }
bool CSharpSourceDocument::Undo() { const auto result = text_->Undo(); if (result) NotifyChanged(); return result; }
bool CSharpSourceDocument::Redo() { const auto result = text_->Redo(); if (result) NotifyChanged(); return result; }
bool CSharpSourceDocument::RecoverAsDirtyBuffer()
{ const auto result = text_->RecoverAsDirtyBuffer(); if (result) NotifyChanged(); return result; }
bool CSharpSourceDocument::DiscardRecovery()
{ const auto result = text_->DiscardRecovery(); if (result) NotifyChanged(); return result; }
CSharpSourceDocument::ChangeSubscription CSharpSourceDocument::SubscribeChanges(ChangeCallback callback)
{
    if (!callback) return InvalidChangeSubscription;
    const auto subscription = nextSubscription_++;
    callbacks_.emplace(subscription, std::move(callback));
    return subscription;
}
void CSharpSourceDocument::UnsubscribeChanges(ChangeSubscription subscription)
{ callbacks_.erase(subscription); }
void CSharpSourceDocument::NotifyChanged()
{
    const auto callbacks = callbacks_;
    for (const auto& [subscription, callback] : callbacks)
    { (void)subscription; if (callback) callback(); }
}

InputMapDocument::InputMapDocument(std::filesystem::path sourcePath)
    : text_(std::make_unique<TextDocument>("input:" + sourcePath.filename().string(),
        DocumentKind::InputMap, std::move(sourcePath), 1, true, ValidateInputMap)) {}
InputMapDocument::~InputMapDocument() = default;
const DocumentSnapshot& InputMapDocument::Snapshot() const noexcept { return text_->Snapshot(); }
const std::vector<DocumentDiagnostic>& InputMapDocument::Diagnostics() const noexcept { return text_->Diagnostics(); }
const std::string& InputMapDocument::Text() const noexcept { return text_->Text(); }
bool InputMapDocument::ReplaceText(std::string text)
{
    if (!text_->ReplaceText(std::move(text))) return false;
    NotifyChanged(); return true;
}
bool InputMapDocument::Load() { const auto result = text_->Load(); if (result) NotifyChanged(); return result; }
DocumentSaveStatus InputMapDocument::Save()
{ const auto result = text_->Save(); if (result == DocumentSaveStatus::Saved) NotifyChanged(); return result; }
bool InputMapDocument::Reload() { const auto result = text_->Reload(); if (result) NotifyChanged(); return result; }
DocumentExternalChangeState InputMapDocument::CheckExternalChange() { return text_->CheckExternalChange(); }
DocumentCloseStatus InputMapDocument::RequestClose() const noexcept { return text_->RequestClose(); }
bool InputMapDocument::CanUndo() const noexcept { return text_->CanUndo(); }
bool InputMapDocument::CanRedo() const noexcept { return text_->CanRedo(); }
bool InputMapDocument::Undo() { const auto result = text_->Undo(); if (result) NotifyChanged(); return result; }
bool InputMapDocument::Redo() { const auto result = text_->Redo(); if (result) NotifyChanged(); return result; }
InputMapDocument::ChangeSubscription InputMapDocument::SubscribeChanges(ChangeCallback callback)
{
    if (!callback) return InvalidChangeSubscription;
    const auto subscription = nextSubscription_++;
    callbacks_.emplace(subscription, std::move(callback));
    return subscription;
}
void InputMapDocument::UnsubscribeChanges(ChangeSubscription subscription)
{ callbacks_.erase(subscription); }
void InputMapDocument::NotifyChanged()
{
    const auto callbacks = callbacks_;
    for (const auto& [subscription, callback] : callbacks)
    { (void)subscription; if (callback) callback(); }
}

#undef SAGA_FORWARD_DOCUMENT

} // namespace SagaEditor
