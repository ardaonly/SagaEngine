/// @file ShortcutCustomizationPanelViewModel.cpp
/// @brief Safe Shortcut Preferences view model implementation.

#include "SagaEditor/Customization/ShortcutCustomizationPanelViewModel.h"

#include "SagaEditor/Customization/WorkspaceCustomizationPanelViewModel.h"

#include <string>
#include <utility>

namespace SagaEditor
{
namespace
{

[[nodiscard]] ShortcutCustomizationActionRow MakeRow(
    const ShortcutCustomizationActionState& action)
{
    ShortcutCustomizationActionRow row;
    row.actionId = action.actionId;
    row.displayName = action.displayName.empty()
                          ? action.actionId
                          : action.displayName;
    row.currentChord = action.currentChord;
    row.pending = action.pendingChord.has_value();
    row.pendingChord = action.pendingChord.value_or(std::string{});
    row.effectiveChord = action.pendingChord.value_or(action.currentChord);
    row.editable = action.editable;
    row.lockedReason = ToDisplayString(action.lockedReason);
    return row;
}

[[nodiscard]] ShortcutCustomizationDiagnosticRow MakeDiagnosticRow(
    const EditorCustomizationDiagnostic& diagnostic)
{
    return { diagnostic.severity, diagnostic.code, diagnostic.message };
}

} // namespace

ShortcutCustomizationPanelViewModel::ShortcutCustomizationPanelViewModel(
    ShortcutCustomizationSession& session)
    : m_session(&session)
{}

ShortcutCustomizationPanelViewState
ShortcutCustomizationPanelViewModel::GetState() const
{
    ShortcutCustomizationPanelViewState state;
    if (m_session == nullptr)
    {
        state.statusText = ToDisplayString(state.status);
        return state;
    }

    const ShortcutCustomizationModel& model = m_session->GetModel();
    state.status = model.status;
    state.statusText = ToDisplayString(model.status);
    state.overlayPath = m_session->OverlayPath().generic_string();
    state.dirty = model.dirty;
    state.restartRequired = model.restartRequired;
    state.canSave =
        model.status == ShortcutCustomizationStatus::Ready &&
        !state.overlayPath.empty();
    state.canReset = !state.overlayPath.empty();

    state.actions.reserve(model.actions.size());
    for (const ShortcutCustomizationActionState& action : model.actions)
    {
        state.actions.push_back(MakeRow(action));
    }

    state.diagnostics.reserve(model.diagnostics.size());
    for (const EditorCustomizationDiagnostic& diagnostic : model.diagnostics)
    {
        state.diagnostics.push_back(MakeDiagnosticRow(diagnostic));
    }
    return state;
}

bool ShortcutCustomizationPanelViewModel::SetShortcut(
    const std::string& actionId,
    const std::string& chord)
{
    if (m_session == nullptr || m_session->GetController() == nullptr)
    {
        return false;
    }
    return m_session->GetController()->SetShortcut(actionId, chord);
}

bool ShortcutCustomizationPanelViewModel::ClearShortcut(
    const std::string& actionId)
{
    if (m_session == nullptr || m_session->GetController() == nullptr)
    {
        return false;
    }
    return m_session->GetController()->ClearShortcut(actionId);
}

bool ShortcutCustomizationPanelViewModel::ResetShortcut(
    const std::string& actionId)
{
    if (m_session == nullptr || m_session->GetController() == nullptr)
    {
        return false;
    }
    return m_session->GetController()->ResetShortcut(actionId);
}

ShortcutCustomizationSessionResult
ShortcutCustomizationPanelViewModel::Save()
{
    if (m_session == nullptr)
    {
        return {};
    }
    return m_session->SaveOverlay();
}

ShortcutCustomizationSessionResult
ShortcutCustomizationPanelViewModel::ResetAll()
{
    if (m_session == nullptr)
    {
        return {};
    }
    return m_session->ResetShortcuts();
}

ShortcutCustomizationSessionResult
ShortcutCustomizationPanelViewModel::Reload()
{
    if (m_session == nullptr)
    {
        return {};
    }
    return m_session->ReloadOverlay();
}

ShortcutCustomizationSessionResult
ShortcutCustomizationPanelViewModel::ExportOverlay(
    const std::filesystem::path& path)
{
    if (m_session == nullptr)
    {
        return {};
    }
    return m_session->ExportOverlay(path);
}

ShortcutCustomizationSessionResult
ShortcutCustomizationPanelViewModel::ImportOverlay(
    const std::filesystem::path& path)
{
    if (m_session == nullptr)
    {
        return {};
    }
    return m_session->ImportOverlay(path);
}

std::string ToDisplayString(ShortcutCustomizationStatus status)
{
    switch (status)
    {
    case ShortcutCustomizationStatus::Unconfigured:
        return "Unconfigured";
    case ShortcutCustomizationStatus::Ready:
        return "Ready";
    case ShortcutCustomizationStatus::Invalid:
        return "Invalid";
    }
    return "Invalid";
}

} // namespace SagaEditor
