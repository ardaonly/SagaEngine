/// @file EditorCompositionShellAdapter.cpp
/// @brief EditorCompositionShellAdapter implementation.

#include "SagaEditor/Composition/EditorCompositionShellAdapter.h"

#include "SagaEditor/Commands/CommandRegistry.h"
#include "SagaEditor/Commands/ShortcutManager.h"
#include "SagaEditor/Composition/ResolvedEditorCompositionSnapshot.h"

#include <algorithm>
#include <cstdint>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace SagaEditor
{
namespace
{

[[nodiscard]] EditorDiagnostic MakeDiagnostic(EditorDiagnosticSeverity severity,
                                              std::string code,
                                              std::string message,
                                              std::string resource = {})
{
    EditorDiagnostic diagnostic;
    diagnostic.severity = severity;
    diagnostic.code = std::move(code);
    diagnostic.message = std::move(message);
    diagnostic.location.resource = std::move(resource);
    return diagnostic;
}

[[nodiscard]] std::vector<std::string> Split(const std::string& text,
                                             char delimiter)
{
    std::vector<std::string> parts;
    std::stringstream stream(text);
    std::string part;
    while (std::getline(stream, part, delimiter))
    {
        parts.push_back(part);
    }
    return parts;
}

[[nodiscard]] KeyChord ParseChord(const std::string& text)
{
    KeyChord chord;
    for (const std::string& token : Split(text, '+'))
    {
        if (token == "Ctrl")
        {
            chord.modifiers |= 1u;
        }
        else if (token == "Shift")
        {
            chord.modifiers |= 2u;
        }
        else if (token == "Alt")
        {
            chord.modifiers |= 4u;
        }
        else if (token == "Super")
        {
            chord.modifiers |= 8u;
        }
        else if (!token.empty())
        {
            chord.keyCode = static_cast<std::uint32_t>(token.front());
        }
    }
    return chord;
}

[[nodiscard]] std::unordered_map<std::string, const ActionDefinition*>
IndexActions(const ResolvedEditorCompositionSnapshot& snapshot)
{
    std::unordered_map<std::string, const ActionDefinition*> actions;
    for (const ActionDefinition& action : snapshot.actions)
    {
        actions[action.id] = &action;
    }
    return actions;
}

[[nodiscard]] std::unordered_map<std::string, std::string> IndexShortcutLabels(
    const ResolvedEditorCompositionSnapshot& snapshot)
{
    std::unordered_map<std::string, std::string> labels;
    for (const ShortcutBindingDefinition& shortcut : snapshot.shortcuts)
    {
        labels[shortcut.actionId] = shortcut.chord;
    }
    return labels;
}

[[nodiscard]] const ResolvedToolbarDefinition* SelectMainToolbar(
    const ResolvedEditorCompositionSnapshot& snapshot)
{
    if (snapshot.toolbars.empty())
    {
        return nullptr;
    }

    auto it = std::find_if(snapshot.toolbars.begin(),
                           snapshot.toolbars.end(),
                           [](const ResolvedToolbarDefinition& toolbar)
                           {
                               return toolbar.definition.id == "saga.toolbar.main";
                           });
    return it != snapshot.toolbars.end() ? &*it : &snapshot.toolbars.front();
}

[[nodiscard]] MenuItemDescriptor MakeActionMenuItem(
    const std::string& actionId,
    const std::unordered_map<std::string, const ActionDefinition*>& actions,
    const std::unordered_map<std::string, std::string>& shortcutLabels,
    const CommandRegistry& commands,
    std::vector<EditorDiagnostic>& diagnostics)
{
    const CommandDescriptor* command = commands.Find(actionId);
    auto actionIt = actions.find(actionId);
    if (actionIt == actions.end())
    {
        diagnostics.push_back(MakeDiagnostic(
            EditorDiagnosticSeverity::Error,
            "CompositionShell.UnknownAction",
            "Composition menu references an action that is absent from the resolved snapshot.",
            actionId));
    }
    if (command == nullptr)
    {
        diagnostics.push_back(MakeDiagnostic(
            EditorDiagnosticSeverity::Error,
            "CompositionShell.UnknownCommand",
            "Composition action has no registered editor command implementation.",
            actionId));
    }

    MenuItemDescriptor item;
    item.commandId = actionId;
    if (command != nullptr)
    {
        item.label = command->label;
    }
    else if (actionIt != actions.end())
    {
        item.label = actionIt->second->displayName;
    }
    else
    {
        item.label = actionId;
    }

    if (auto shortcutIt = shortcutLabels.find(actionId);
        shortcutIt != shortcutLabels.end())
    {
        item.shortcut = shortcutIt->second;
    }
    return item;
}

} // namespace

EditorCompositionShellLayoutResult EditorCompositionShellAdapter::BuildShellLayout(
    const ResolvedEditorCompositionSnapshot& snapshot,
    const CommandRegistry& commands,
    const ShellLayout& fallback) const
{
    EditorCompositionShellLayoutResult result;
    result.layout = fallback;
    result.usedComposition = !snapshot.menus.empty() || !snapshot.toolbars.empty();

    const auto actions = IndexActions(snapshot);
    const auto shortcutLabels = IndexShortcutLabels(snapshot);

    if (!snapshot.menus.empty())
    {
        result.layout.menus.clear();
        for (const MenuDefinition& menuDefinition : snapshot.menus)
        {
            MenuDescriptor menu;
            menu.title = menuDefinition.displayName.empty()
                         ? menuDefinition.id
                         : menuDefinition.displayName;
            for (const MenuItemDefinition& itemDefinition : menuDefinition.items)
            {
                if (itemDefinition.kind == "separator")
                {
                    menu.items.push_back({ {}, {}, {}, true });
                }
                else if (itemDefinition.kind == "action")
                {
                    menu.items.push_back(MakeActionMenuItem(itemDefinition.actionId,
                                                            actions,
                                                            shortcutLabels,
                                                            commands,
                                                            result.diagnostics));
                }
                else if (itemDefinition.kind == "menu")
                {
                    result.diagnostics.push_back(MakeDiagnostic(
                        EditorDiagnosticSeverity::Warning,
                        "CompositionShell.SubmenuUnsupported",
                        "Composition submenu entries are not representable by the current shell layout.",
                        itemDefinition.menuId));
                }
                else
                {
                    result.diagnostics.push_back(MakeDiagnostic(
                        EditorDiagnosticSeverity::Error,
                        "CompositionShell.UnknownMenuItemKind",
                        "Composition menu item kind is not supported by the shell adapter.",
                        itemDefinition.kind));
                }
            }
            result.layout.menus.push_back(std::move(menu));
        }
    }

    if (const ResolvedToolbarDefinition* toolbar = SelectMainToolbar(snapshot))
    {
        result.layout.mainToolbarItems.clear();
        for (const std::string& actionId : toolbar->visibleActionIds)
        {
            result.layout.mainToolbarItems.push_back(MakeActionMenuItem(actionId,
                                                                        actions,
                                                                        shortcutLabels,
                                                                        commands,
                                                                        result.diagnostics));
        }
    }

    return result;
}

std::vector<EditorDiagnostic> EditorCompositionShellAdapter::ApplyShortcuts(
    const ResolvedEditorCompositionSnapshot& snapshot,
    const CommandRegistry& commands,
    ShortcutManager& shortcuts) const
{
    std::vector<EditorDiagnostic> diagnostics;
    shortcuts.Clear();

    for (const ShortcutBindingDefinition& shortcut : snapshot.shortcuts)
    {
        if (commands.Find(shortcut.actionId) == nullptr)
        {
            diagnostics.push_back(MakeDiagnostic(
                EditorDiagnosticSeverity::Error,
                "CompositionShell.UnknownShortcutCommand",
                "Composition shortcut references an action with no registered editor command implementation.",
                shortcut.actionId));
            continue;
        }

        const KeyChord chord = ParseChord(shortcut.chord);
        if (chord.keyCode == 0)
        {
            diagnostics.push_back(MakeDiagnostic(
                EditorDiagnosticSeverity::Error,
                "CompositionShell.InvalidShortcutChord",
                "Composition shortcut chord could not be parsed.",
                shortcut.chord));
            continue;
        }

        shortcuts.Bind(chord, shortcut.actionId);
    }

    return diagnostics;
}

std::vector<EditorDiagnostic> EditorCompositionShellAdapter::ValidateRegisteredPanels(
    const ResolvedEditorCompositionSnapshot& snapshot,
    const std::vector<std::string>& registeredPanelIds) const
{
    std::vector<EditorDiagnostic> diagnostics;
    std::unordered_set<std::string> registered(registeredPanelIds.begin(),
                                               registeredPanelIds.end());
    for (const ResolvedPanelDefinition& panel : snapshot.panels)
    {
        if (!registered.contains(panel.definition.id))
        {
            diagnostics.push_back(MakeDiagnostic(
                EditorDiagnosticSeverity::Error,
                "CompositionShell.MissingPanelImplementation",
                "Composition panel has no registered editor panel implementation.",
                panel.definition.id));
        }
    }
    return diagnostics;
}

} // namespace SagaEditor
