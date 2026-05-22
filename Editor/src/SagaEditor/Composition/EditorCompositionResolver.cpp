/// @file EditorCompositionResolver.cpp
/// @brief Resolves compiled editor composition artifacts with safe overlays.

#include "SagaEditor/Composition/EditorCompositionResolver.h"

#include <algorithm>
#include <map>
#include <set>
#include <unordered_map>
#include <unordered_set>

namespace SagaEditor
{
namespace
{

void AddDiagnostic(std::vector<EditorCompositionDiagnostic>& diagnostics,
                   EditorCompositionDiagnosticSeverity severity,
                   std::string code,
                   std::string message,
                   std::string jsonPointer,
                   std::string referencedId = {})
{
    diagnostics.push_back({ severity,
                            std::move(code),
                            std::move(message),
                            {},
                            std::move(jsonPointer),
                            std::move(referencedId) });
}

template <typename T>
void IndexDefinitions(const std::vector<T>& definitions,
                      const std::string& typeName,
                      const std::string& path,
                      std::unordered_map<std::string, const T*>& index,
                      std::vector<EditorCompositionDiagnostic>& diagnostics)
{
    for (const T& definition : definitions)
    {
        if (definition.id.empty())
        {
            AddDiagnostic(diagnostics,
                          EditorCompositionDiagnosticSeverity::Error,
                          "CompositionArtifact.EmptyId",
                          "Compiled composition definition has an empty id.",
                          path);
            continue;
        }

        if (definition.displayName.empty())
        {
            AddDiagnostic(diagnostics,
                          EditorCompositionDiagnosticSeverity::Error,
                          "CompositionArtifact.EmptyDisplayName",
                          "Compiled composition definition has an empty display name.",
                          path,
                          definition.id);
        }

        if (index.contains(definition.id))
        {
            AddDiagnostic(diagnostics,
                          EditorCompositionDiagnosticSeverity::Blocker,
                          "CompositionArtifact.DuplicateId",
                          "Compiled composition artifact contains a duplicate " + typeName + " id.",
                          path,
                          definition.id);
            continue;
        }

        index[definition.id] = &definition;
    }
}

template <typename T>
[[nodiscard]] bool HasDefinition(const std::unordered_map<std::string, const T*>& index,
                                 const std::string& id)
{
    return index.find(id) != index.end();
}

[[nodiscard]] bool IsActionSafe(const std::unordered_map<std::string, const ActionDefinition*>& actions,
                                const std::string& actionId)
{
    auto it = actions.find(actionId);
    return it != actions.end() && it->second->safeInProduct && !it->second->internalOnly;
}

void ValidateArtifactReferences(
    const EditorCompositionArtifact& artifact,
    const std::unordered_map<std::string, const PanelDefinition*>& panels,
    const std::unordered_map<std::string, const ActionDefinition*>& actions,
    const std::unordered_map<std::string, const MenuDefinition*>& menus,
    const std::unordered_map<std::string, const ToolbarDefinition*>& toolbars,
    const std::unordered_map<std::string, const WorkspaceLayoutDefinition*>& layouts,
    const std::unordered_map<std::string, const WorkspaceProfileDefinition*>& workspaces,
    std::vector<EditorCompositionDiagnostic>& diagnostics)
{
    (void)toolbars;

    for (const MenuDefinition& menu : artifact.menus)
    {
        for (const MenuItemDefinition& item : menu.items)
        {
            if (item.kind == "action" && !HasDefinition(actions, item.actionId))
            {
                AddDiagnostic(diagnostics,
                              EditorCompositionDiagnosticSeverity::Error,
                              "CompositionArtifact.MissingActionReference",
                              "Menu references an action that is not present in the artifact.",
                              "/menus",
                              item.actionId);
            }
            else if (item.kind == "menu" && !HasDefinition(menus, item.menuId))
            {
                AddDiagnostic(diagnostics,
                              EditorCompositionDiagnosticSeverity::Error,
                              "CompositionArtifact.MissingMenuReference",
                              "Menu references a submenu that is not present in the artifact.",
                              "/menus",
                              item.menuId);
            }
            else if (item.kind != "action" && item.kind != "menu" && item.kind != "separator")
            {
                AddDiagnostic(diagnostics,
                              EditorCompositionDiagnosticSeverity::Error,
                              "CompositionArtifact.UnknownMenuItemKind",
                              "Menu item kind is not supported by the editor composition consumer.",
                              "/menus",
                              item.kind);
            }
        }
    }

    for (const ToolbarDefinition& toolbar : artifact.toolbars)
    {
        for (const std::string& actionId : toolbar.actions)
        {
            if (!HasDefinition(actions, actionId))
            {
                AddDiagnostic(diagnostics,
                              EditorCompositionDiagnosticSeverity::Error,
                              "CompositionArtifact.MissingActionReference",
                              "Toolbar references an action that is not present in the artifact.",
                              "/toolbars",
                              actionId);
            }
        }
    }

    for (const ShortcutBindingDefinition& shortcut : artifact.shortcuts)
    {
        if (!HasDefinition(actions, shortcut.actionId))
        {
            AddDiagnostic(diagnostics,
                          EditorCompositionDiagnosticSeverity::Error,
                          "CompositionArtifact.MissingActionReference",
                          "Shortcut references an action that is not present in the artifact.",
                          "/shortcuts",
                          shortcut.actionId);
        }
    }

    for (const WorkspaceProfileDefinition& workspace : artifact.workspaces)
    {
        if (!workspace.layoutId.empty() && !HasDefinition(layouts, workspace.layoutId))
        {
            AddDiagnostic(diagnostics,
                          EditorCompositionDiagnosticSeverity::Error,
                          "CompositionArtifact.MissingLayoutReference",
                          "Workspace references a layout that is not present in the artifact.",
                          "/workspaces",
                          workspace.layoutId);
        }

        for (const std::string& panelId : workspace.visiblePanels)
        {
            if (!HasDefinition(panels, panelId))
            {
                AddDiagnostic(diagnostics,
                              EditorCompositionDiagnosticSeverity::Error,
                              "CompositionArtifact.MissingPanelReference",
                              "Workspace references a panel that is not present in the artifact.",
                              "/workspaces",
                              panelId);
            }
        }
    }

    for (const EditorModeDefinition& mode : artifact.editorModes)
    {
        if (!HasDefinition(workspaces, mode.workspaceId))
        {
            AddDiagnostic(diagnostics,
                          EditorCompositionDiagnosticSeverity::Error,
                          "CompositionArtifact.MissingWorkspaceReference",
                          "Editor mode references a workspace that is not present in the artifact.",
                          "/editorModes",
                          mode.workspaceId);
        }

        for (const std::string& panelId : mode.requiredPanels)
        {
            if (!HasDefinition(panels, panelId))
            {
                AddDiagnostic(diagnostics,
                              EditorCompositionDiagnosticSeverity::Error,
                              "CompositionArtifact.MissingPanelReference",
                              "Editor mode references a panel that is not present in the artifact.",
                              "/editorModes",
                              panelId);
            }
        }
    }
}

} // namespace

ResolvedEditorCompositionSnapshot EditorCompositionResolver::Resolve(
    const EditorCompositionManifest& manifest,
    const EditorCompositionArtifact& artifact,
    std::span<const EditorCustomizationOverlay> overlays) const
{
    ResolvedEditorCompositionSnapshot snapshot;
    snapshot.compositionId = artifact.compositionId;
    snapshot.artifactHash = manifest.artifactHash;
    snapshot.actions = artifact.actions;
    snapshot.menus = artifact.menus;
    snapshot.workspaces = artifact.workspaces;
    snapshot.editorModes = artifact.editorModes;

    std::unordered_map<std::string, const PanelDefinition*> panels;
    std::unordered_map<std::string, const ActionDefinition*> actions;
    std::unordered_map<std::string, const MenuDefinition*> menus;
    std::unordered_map<std::string, const ToolbarDefinition*> toolbars;
    std::unordered_map<std::string, const WorkspaceLayoutDefinition*> layouts;
    std::unordered_map<std::string, const WorkspaceProfileDefinition*> workspaces;

    IndexDefinitions(artifact.panels, "panel", "/panels", panels, snapshot.diagnostics);
    IndexDefinitions(artifact.actions, "action", "/actions", actions, snapshot.diagnostics);
    IndexDefinitions(artifact.menus, "menu", "/menus", menus, snapshot.diagnostics);
    IndexDefinitions(artifact.toolbars, "toolbar", "/toolbars", toolbars, snapshot.diagnostics);
    IndexDefinitions(artifact.workspaceLayouts, "workspaceLayout", "/workspaceLayouts", layouts, snapshot.diagnostics);
    IndexDefinitions(artifact.workspaces, "workspace", "/workspaces", workspaces, snapshot.diagnostics);

    ValidateArtifactReferences(artifact, panels, actions, menus, toolbars, layouts, workspaces, snapshot.diagnostics);

    std::map<std::string, ResolvedPanelDefinition> resolvedPanelsById;
    for (const PanelDefinition& panel : artifact.panels)
    {
        resolvedPanelsById[panel.id] = { panel, panel.defaultVisible, {} };
    }

    std::vector<ShortcutBindingDefinition> resolvedShortcuts = artifact.shortcuts;
    std::map<std::string, ResolvedToolbarDefinition> resolvedToolbarsById;
    for (const ToolbarDefinition& toolbar : artifact.toolbars)
    {
        resolvedToolbarsById[toolbar.id] = { toolbar, toolbar.actions };
    }

    for (const EditorCustomizationOverlay& overlay : overlays)
    {
        snapshot.appliedOverlayIds.push_back(overlay.overlayId);

        if (overlay.baseCompositionId != artifact.compositionId)
        {
            AddDiagnostic(snapshot.diagnostics,
                          EditorCompositionDiagnosticSeverity::Blocker,
                          "CompositionOverlay.BaseCompositionMismatch",
                          "Overlay targets a different base composition.",
                          "/baseCompositionId",
                          overlay.baseCompositionId);
            continue;
        }

        if (!overlay.baseArtifactHash.empty() &&
            !manifest.artifactHash.empty() &&
            overlay.baseArtifactHash != manifest.artifactHash)
        {
            AddDiagnostic(snapshot.diagnostics,
                          EditorCompositionDiagnosticSeverity::Error,
                          "CompositionOverlay.BaseArtifactHashMismatch",
                          "Overlay targets a different compiled artifact hash.",
                          "/baseArtifactHash",
                          overlay.baseArtifactHash);
        }

        for (const LayoutCustomizationOverride& overrideValue : overlay.layoutOverrides)
        {
            auto panelIt = resolvedPanelsById.find(overrideValue.panelId);
            if (panelIt == resolvedPanelsById.end())
            {
                AddDiagnostic(snapshot.diagnostics,
                              EditorCompositionDiagnosticSeverity::Error,
                              "CompositionOverlay.UnknownPanel",
                              "Layout override references a panel that is not present in the artifact.",
                              "/layoutOverrides",
                              overrideValue.panelId);
                continue;
            }
            if (!HasDefinition(workspaces, overrideValue.workspaceId))
            {
                AddDiagnostic(snapshot.diagnostics,
                              EditorCompositionDiagnosticSeverity::Error,
                              "CompositionOverlay.UnknownWorkspace",
                              "Layout override references a workspace that is not present in the artifact.",
                              "/layoutOverrides",
                              overrideValue.workspaceId);
                continue;
            }
            if (panelIt->second.definition.internalOnly)
            {
                AddDiagnostic(snapshot.diagnostics,
                              EditorCompositionDiagnosticSeverity::Blocker,
                              "CompositionOverlay.UnsafeMutation",
                              "Safe overlays cannot mutate internal-only panels.",
                              "/layoutOverrides",
                              overrideValue.panelId);
                continue;
            }

            panelIt->second.visible = overrideValue.visible;
            panelIt->second.placement = overrideValue.placement;
        }

        for (const VisibilityCustomizationOverride& overrideValue : overlay.visibilityOverrides)
        {
            auto panelIt = resolvedPanelsById.find(overrideValue.panelId);
            if (panelIt == resolvedPanelsById.end())
            {
                AddDiagnostic(snapshot.diagnostics,
                              EditorCompositionDiagnosticSeverity::Error,
                              "CompositionOverlay.UnknownPanel",
                              "Visibility override references a panel that is not present in the artifact.",
                              "/visibilityOverrides",
                              overrideValue.panelId);
                continue;
            }
            if (panelIt->second.definition.internalOnly)
            {
                AddDiagnostic(snapshot.diagnostics,
                              EditorCompositionDiagnosticSeverity::Blocker,
                              "CompositionOverlay.UnsafeMutation",
                              "Safe overlays cannot mutate internal-only panels.",
                              "/visibilityOverrides",
                              overrideValue.panelId);
                continue;
            }

            panelIt->second.visible = overrideValue.visible;
        }

        for (const ShortcutCustomizationOverride& overrideValue : overlay.shortcutOverrides)
        {
            if (!IsActionSafe(actions, overrideValue.actionId))
            {
                AddDiagnostic(snapshot.diagnostics,
                              EditorCompositionDiagnosticSeverity::Blocker,
                              "CompositionOverlay.UnsafeMutation",
                              "Safe overlays cannot override unknown or internal-only actions.",
                              "/shortcutOverrides",
                              overrideValue.actionId);
                continue;
            }

            auto existing = std::find_if(resolvedShortcuts.begin(),
                                         resolvedShortcuts.end(),
                                         [&](const ShortcutBindingDefinition& shortcut)
                                         {
                                             return shortcut.actionId == overrideValue.actionId;
                                         });
            if (existing != resolvedShortcuts.end())
            {
                existing->chord = overrideValue.chord;
            }
            else
            {
                resolvedShortcuts.push_back({ overlay.overlayId + "." + overrideValue.actionId,
                                              overrideValue.actionId,
                                              overrideValue.chord });
            }
        }

        for (const ToolbarCustomizationOverride& overrideValue : overlay.toolbarOverrides)
        {
            auto toolbarIt = resolvedToolbarsById.find(overrideValue.toolbarId);
            if (toolbarIt == resolvedToolbarsById.end())
            {
                AddDiagnostic(snapshot.diagnostics,
                              EditorCompositionDiagnosticSeverity::Error,
                              "CompositionOverlay.UnknownToolbar",
                              "Toolbar override references a toolbar that is not present in the artifact.",
                              "/toolbarOverrides",
                              overrideValue.toolbarId);
                continue;
            }

            std::set<std::string> hidden(overrideValue.hiddenActionIds.begin(),
                                         overrideValue.hiddenActionIds.end());
            for (const std::string& actionId : hidden)
            {
                if (!IsActionSafe(actions, actionId))
                {
                    AddDiagnostic(snapshot.diagnostics,
                                  EditorCompositionDiagnosticSeverity::Blocker,
                                  "CompositionOverlay.UnsafeMutation",
                                  "Safe overlays cannot mutate unknown or internal-only toolbar actions.",
                                  "/toolbarOverrides",
                                  actionId);
                }
            }

            std::vector<std::string> visibleActions;
            for (const std::string& actionId : toolbarIt->second.definition.actions)
            {
                if (!hidden.contains(actionId))
                {
                    visibleActions.push_back(actionId);
                }
            }
            toolbarIt->second.visibleActionIds = std::move(visibleActions);
        }
    }

    std::map<std::string, std::string> chordOwners;
    for (const ShortcutBindingDefinition& shortcut : resolvedShortcuts)
    {
        if (shortcut.chord.empty())
        {
            continue;
        }

        auto [it, inserted] = chordOwners.emplace(shortcut.chord, shortcut.actionId);
        if (!inserted && it->second != shortcut.actionId)
        {
            AddDiagnostic(snapshot.diagnostics,
                          EditorCompositionDiagnosticSeverity::Error,
                          "CompositionOverlay.ShortcutCollision",
                          "Resolved shortcut chord is assigned to more than one action.",
                          "/shortcutOverrides",
                          shortcut.chord);
        }
    }

    for (const auto& [_, panel] : resolvedPanelsById)
    {
        snapshot.panels.push_back(panel);
    }
    for (const auto& [_, toolbar] : resolvedToolbarsById)
    {
        snapshot.toolbars.push_back(toolbar);
    }
    snapshot.shortcuts = std::move(resolvedShortcuts);

    snapshot.isUsable = !HasErrorCompositionDiagnostic(snapshot.diagnostics);
    return snapshot;
}

} // namespace SagaEditor
