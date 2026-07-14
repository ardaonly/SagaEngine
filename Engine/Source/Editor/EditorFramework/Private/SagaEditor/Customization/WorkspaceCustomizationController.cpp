/// @file WorkspaceCustomizationController.cpp
/// @brief Workspace customization controller implementation.

#include "SagaEditor/Customization/WorkspaceCustomizationController.h"

#include "SagaEditor/Composition/ResolvedEditorCompositionSnapshot.h"

#include <algorithm>
#include <utility>

namespace SagaEditor
{
namespace
{

[[nodiscard]] bool SamePanelOverride(const VisibilityCustomizationOverride& value,
                                     const std::string& panelId)
{
    return value.panelId == panelId;
}

} // namespace

WorkspaceCustomizationController::WorkspaceCustomizationController(
    WorkspaceCustomizationControllerConfig config)
    : m_capabilities(BuildEditorCustomizationCapabilities(config.snapshot,
                                                          config.availability))
    , m_overlayId(std::move(config.overlayId))
{
    if (config.initialOverlay)
    {
        m_passthroughShortcuts = config.initialOverlay->shortcutOverrides;
        m_passthroughToolbars = config.initialOverlay->toolbarOverrides;

        if (config.initialOverlay->baseCompositionId !=
            m_capabilities.compositionId)
        {
            AddDiagnostic(EditorCustomizationDiagnosticSeverity::Blocker,
                          "Customization.InvalidOverlay",
                          "Initial customization overlay targets a different base composition.",
                          config.initialOverlay->baseCompositionId);
        }
        else
        {
            for (const LayoutCustomizationOverride& overrideValue :
                 config.initialOverlay->layoutOverrides)
            {
                m_pendingVisibility.push_back({ overrideValue.panelId,
                                                overrideValue.visible });
            }
            for (const VisibilityCustomizationOverride& overrideValue :
                 config.initialOverlay->visibilityOverrides)
            {
                auto it = std::find_if(
                    m_pendingVisibility.begin(),
                    m_pendingVisibility.end(),
                    [&](const VisibilityCustomizationOverride& pending)
                    {
                        return pending.panelId == overrideValue.panelId;
                    });
                if (it != m_pendingVisibility.end())
                {
                    it->visible = overrideValue.visible;
                }
                else
                {
                    m_pendingVisibility.push_back(overrideValue);
                }
            }
        }
    }

    RebuildModel();
}

bool WorkspaceCustomizationController::TogglePanelVisibility(
    const std::string& panelId,
    bool visible)
{
    WorkspaceCustomizationPanelState* panel = FindPanelState(panelId);
    if (panel == nullptr)
    {
        AddDiagnostic(EditorCustomizationDiagnosticSeverity::Error,
                      "Customization.UnknownPanel",
                      "Cannot customize an unknown panel.",
                      panelId);
        RebuildModel();
        return false;
    }

    const PanelCustomizationCapability* capability =
        FindPanelCapability(panelId);
    if (capability == nullptr || !capability->editable)
    {
        const std::string code =
            capability != nullptr && capability->internalOnly
                ? "Customization.InternalOnlyPanel"
                : capability != nullptr &&
                  capability->lockedReason ==
                      EditorCustomizationLockReason::UnavailableImplementation
                    ? "Customization.UnavailablePanelImplementation"
                    : "Customization.PanelLocked";
        AddDiagnostic(EditorCustomizationDiagnosticSeverity::Error,
                      code,
                      "Cannot customize a locked panel.",
                      panelId);
        RebuildModel();
        return false;
    }

    auto it = std::find_if(m_pendingVisibility.begin(),
                           m_pendingVisibility.end(),
                           [&](const VisibilityCustomizationOverride& value)
                           {
                               return SamePanelOverride(value, panelId);
                           });
    if (visible == panel->currentVisible)
    {
        if (it != m_pendingVisibility.end())
        {
            m_pendingVisibility.erase(it);
        }
    }
    else if (it != m_pendingVisibility.end())
    {
        it->visible = visible;
    }
    else
    {
        m_pendingVisibility.push_back({ panelId, visible });
    }

    RebuildModel();
    return true;
}

bool WorkspaceCustomizationController::ResetPanel(const std::string& panelId)
{
    if (FindPanelState(panelId) == nullptr)
    {
        AddDiagnostic(EditorCustomizationDiagnosticSeverity::Error,
                      "Customization.UnknownPanel",
                      "Cannot reset an unknown panel.",
                      panelId);
        RebuildModel();
        return false;
    }

    const auto oldSize = m_pendingVisibility.size();
    m_pendingVisibility.erase(
        std::remove_if(m_pendingVisibility.begin(),
                       m_pendingVisibility.end(),
                       [&](const VisibilityCustomizationOverride& value)
                       {
                           return SamePanelOverride(value, panelId);
                       }),
        m_pendingVisibility.end());
    RebuildModel();
    return oldSize != m_pendingVisibility.size();
}

void WorkspaceCustomizationController::ResetWorkspace()
{
    m_pendingVisibility.clear();
    RebuildModel();
}

const WorkspaceCustomizationModel&
WorkspaceCustomizationController::GetModel() const noexcept
{
    return m_model;
}

const std::vector<EditorCustomizationDiagnostic>&
WorkspaceCustomizationController::GetDiagnostics() const noexcept
{
    return m_diagnostics;
}

bool WorkspaceCustomizationController::Validate()
{
    bool valid = m_capabilities.hasUsableSnapshot;
    if (!valid)
    {
        AddDiagnostic(EditorCustomizationDiagnosticSeverity::Blocker,
                      "Customization.NoCompositionSnapshot",
                      "Cannot validate customization without a usable snapshot.");
    }

    for (const VisibilityCustomizationOverride& overrideValue :
         m_pendingVisibility)
    {
        const PanelCustomizationCapability* capability =
            FindPanelCapability(overrideValue.panelId);
        if (capability == nullptr)
        {
            valid = false;
            AddDiagnostic(EditorCustomizationDiagnosticSeverity::Error,
                          "Customization.UnknownPanel",
                          "Customization overlay references an unknown panel.",
                          overrideValue.panelId);
        }
        else if (!capability->editable)
        {
            valid = false;
            AddDiagnostic(EditorCustomizationDiagnosticSeverity::Error,
                          capability->internalOnly
                              ? "Customization.InternalOnlyPanel"
                              : "Customization.PanelLocked",
                          "Customization overlay references a locked panel.",
                          overrideValue.panelId);
        }
    }

    RebuildModel();
    return valid && !HasErrorCustomizationDiagnostic(m_diagnostics);
}

EditorCustomizationOverlay
WorkspaceCustomizationController::BuildOverlayDelta() const
{
    EditorCustomizationOverlay overlay;
    overlay.schemaVersion = kCurrentEditorCustomizationOverlayVersion;
    overlay.baseCompositionId = m_capabilities.compositionId;
    overlay.baseArtifactHash = m_capabilities.artifactHash;
    overlay.overlayId = m_overlayId;
    overlay.visibilityOverrides = m_pendingVisibility;
    overlay.shortcutOverrides = m_passthroughShortcuts;
    overlay.toolbarOverrides = m_passthroughToolbars;
    return overlay;
}

void WorkspaceCustomizationController::RebuildModel()
{
    WorkspaceCustomizationModel model;
    model.compositionId = m_capabilities.compositionId;
    model.artifactHash = m_capabilities.artifactHash;
    model.activeWorkspaceId = m_capabilities.activeWorkspaceId;
    model.diagnostics = m_capabilities.diagnostics;
    model.diagnostics.insert(model.diagnostics.end(),
                             m_diagnostics.begin(),
                             m_diagnostics.end());
    model.status = !m_capabilities.hasUsableSnapshot
                       ? WorkspaceCustomizationStatus::Unconfigured
                       : HasErrorCustomizationDiagnostic(model.diagnostics)
                             ? WorkspaceCustomizationStatus::Invalid
                             : WorkspaceCustomizationStatus::Ready;

    for (const PanelCustomizationCapability& capability :
         m_capabilities.panels)
    {
        WorkspaceCustomizationPanelState panel;
        panel.id = capability.id;
        panel.displayName = capability.displayName;
        panel.defaultVisible = capability.defaultVisible;
        panel.currentVisible = capability.currentVisible;
        panel.editable = capability.editable;
        panel.lockedReason = capability.lockedReason;

        auto it = std::find_if(m_pendingVisibility.begin(),
                               m_pendingVisibility.end(),
                               [&](const VisibilityCustomizationOverride& value)
                               {
                                   return SamePanelOverride(value,
                                                            capability.id);
                               });
        if (it != m_pendingVisibility.end())
        {
            panel.pendingVisible = it->visible;
        }

        model.panels.push_back(std::move(panel));
    }

    model.actions = m_capabilities.actions;
    model.workspaces = m_capabilities.workspaces;
    model.dirty = !m_pendingVisibility.empty();
    model.liveApplyAvailable = false;
    model.restartRequired = model.dirty;
    m_model = std::move(model);
}

WorkspaceCustomizationPanelState*
WorkspaceCustomizationController::FindPanelState(const std::string& panelId)
{
    auto it = std::find_if(m_model.panels.begin(),
                           m_model.panels.end(),
                           [&](const WorkspaceCustomizationPanelState& panel)
                           {
                               return panel.id == panelId;
                           });
    return it == m_model.panels.end() ? nullptr : &*it;
}

const WorkspaceCustomizationPanelState*
WorkspaceCustomizationController::FindPanelState(
    const std::string& panelId) const
{
    auto it = std::find_if(m_model.panels.begin(),
                           m_model.panels.end(),
                           [&](const WorkspaceCustomizationPanelState& panel)
                           {
                               return panel.id == panelId;
                           });
    return it == m_model.panels.end() ? nullptr : &*it;
}

const PanelCustomizationCapability*
WorkspaceCustomizationController::FindPanelCapability(
    const std::string& panelId) const
{
    auto it = std::find_if(m_capabilities.panels.begin(),
                           m_capabilities.panels.end(),
                           [&](const PanelCustomizationCapability& panel)
                           {
                               return panel.id == panelId;
                           });
    return it == m_capabilities.panels.end() ? nullptr : &*it;
}

void WorkspaceCustomizationController::AddDiagnostic(
    EditorCustomizationDiagnosticSeverity severity,
    std::string code,
    std::string message,
    std::string targetId)
{
    AddCustomizationDiagnostic(m_diagnostics,
                               severity,
                               std::move(code),
                               std::move(message),
                               std::move(targetId));
}

} // namespace SagaEditor
