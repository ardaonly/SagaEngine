/// @file WorkspaceCustomizationPanelViewModel.cpp
/// @brief Safe Customize Workspace view model implementation.

#include "SagaEditor/Customization/WorkspaceCustomizationPanelViewModel.h"

#include <utility>

namespace SagaEditor
{
namespace
{

[[nodiscard]] WorkspaceCustomizationPanelRow MakeRow(
    const WorkspaceCustomizationPanelState& panel)
{
    WorkspaceCustomizationPanelRow row;
    row.id = panel.id;
    row.displayName = panel.displayName.empty() ? panel.id : panel.displayName;
    row.currentVisible = panel.currentVisible;
    row.defaultVisible = panel.defaultVisible;
    row.pending = panel.pendingVisible.has_value();
    row.visible = panel.pendingVisible.value_or(panel.currentVisible);
    row.editable = panel.editable;
    row.lockedReason = ToDisplayString(panel.lockedReason);
    return row;
}

[[nodiscard]] WorkspaceCustomizationDiagnosticRow MakeDiagnosticRow(
    const EditorCustomizationDiagnostic& diagnostic)
{
    return { diagnostic.severity, diagnostic.code, diagnostic.message };
}

} // namespace

WorkspaceCustomizationPanelViewModel::WorkspaceCustomizationPanelViewModel(
    WorkspaceCustomizationSession& session)
    : m_session(&session)
{}

WorkspaceCustomizationPanelViewState
WorkspaceCustomizationPanelViewModel::GetState() const
{
    WorkspaceCustomizationPanelViewState state;
    if (m_session == nullptr)
    {
        state.statusText = ToDisplayString(state.status);
        return state;
    }

    const WorkspaceCustomizationModel& model = m_session->GetModel();
    state.status = model.status;
    state.statusText = ToDisplayString(model.status);
    state.overlayPath = m_session->OverlayPath().generic_string();
    state.dirty = model.dirty;
    state.restartRequired = model.restartRequired;
    state.canSave =
        model.status == WorkspaceCustomizationStatus::Ready &&
        !state.overlayPath.empty();
    state.canReset = !state.overlayPath.empty();

    state.panels.reserve(model.panels.size());
    for (const WorkspaceCustomizationPanelState& panel : model.panels)
    {
        state.panels.push_back(MakeRow(panel));
    }

    state.diagnostics.reserve(model.diagnostics.size());
    for (const EditorCustomizationDiagnostic& diagnostic : model.diagnostics)
    {
        state.diagnostics.push_back(MakeDiagnosticRow(diagnostic));
    }
    return state;
}

bool WorkspaceCustomizationPanelViewModel::TogglePanel(
    const std::string& panelId,
    bool visible)
{
    if (m_session == nullptr || m_session->GetController() == nullptr)
    {
        return false;
    }

    return m_session->GetController()->TogglePanelVisibility(panelId, visible);
}

WorkspaceCustomizationSessionResult
WorkspaceCustomizationPanelViewModel::Save()
{
    if (m_session == nullptr)
    {
        return {};
    }
    return m_session->SaveOverlay();
}

WorkspaceCustomizationSessionResult
WorkspaceCustomizationPanelViewModel::ResetWorkspace()
{
    if (m_session == nullptr)
    {
        return {};
    }
    return m_session->ResetOverlay();
}

WorkspaceCustomizationSessionResult
WorkspaceCustomizationPanelViewModel::Reload()
{
    if (m_session == nullptr)
    {
        return {};
    }
    return m_session->ReloadOverlay();
}

WorkspaceCustomizationSessionResult
WorkspaceCustomizationPanelViewModel::ExportOverlay(
    const std::filesystem::path& path)
{
    if (m_session == nullptr)
    {
        return {};
    }
    return m_session->ExportOverlay(path);
}

WorkspaceCustomizationSessionResult
WorkspaceCustomizationPanelViewModel::ImportOverlay(
    const std::filesystem::path& path)
{
    if (m_session == nullptr)
    {
        return {};
    }
    return m_session->ImportOverlay(path);
}

std::string ToDisplayString(WorkspaceCustomizationStatus status)
{
    switch (status)
    {
    case WorkspaceCustomizationStatus::Unconfigured:
        return "Unconfigured";
    case WorkspaceCustomizationStatus::Ready:
        return "Ready";
    case WorkspaceCustomizationStatus::Invalid:
        return "Invalid";
    }
    return "Invalid";
}

std::string ToDisplayString(EditorCustomizationLockReason reason)
{
    switch (reason)
    {
    case EditorCustomizationLockReason::None:
        return {};
    case EditorCustomizationLockReason::NoCompositionSnapshot:
        return "No composition snapshot";
    case EditorCustomizationLockReason::InternalOnly:
        return "Internal only";
    case EditorCustomizationLockReason::UnavailableImplementation:
        return "Implementation unavailable";
    case EditorCustomizationLockReason::NotProductSafe:
        return "Not product safe";
    }
    return "Locked";
}

} // namespace SagaEditor
