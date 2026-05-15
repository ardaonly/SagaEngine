/// @file EditorProfile.cpp
/// @brief Built-in editor workflow profile descriptors.

#include "SagaEditor/Profile/EditorProfile.h"

#include <cstdint>
#include <utility>

namespace SagaEditor
{

namespace
{

constexpr uint32_t kCtrl = 1u;
constexpr uint32_t kShift = 2u;

[[nodiscard]] EditorShortcutBinding Shortcut(uint32_t modifiers,
                                             char key,
                                             std::string commandId,
                                             std::string displayText)
{
    return { KeyChord{ modifiers, static_cast<uint32_t>(key) },
             std::move(commandId),
             std::move(displayText) };
}

[[nodiscard]] std::vector<EditorShortcutBinding> StandardShortcuts()
{
    return {
        Shortcut(kCtrl, 'N', "saga.command.file.new_scene", "Ctrl+N"),
        Shortcut(kCtrl, 'O', "saga.command.file.open_project", "Ctrl+O"),
        Shortcut(kCtrl, 'S', "saga.command.file.save", "Ctrl+S"),
        Shortcut(kCtrl, 'Z', "saga.command.edit.undo", "Ctrl+Z"),
        Shortcut(kCtrl | kShift, 'Z', "saga.command.edit.redo", "Ctrl+Shift+Z"),
    };
}

[[nodiscard]] std::vector<EditorShortcutBinding> NodeEditorShortcuts()
{
    auto shortcuts = StandardShortcuts();
    shortcuts.push_back(
        Shortcut(kCtrl | kShift, 'G', "saga.command.view.graph", "Ctrl+Shift+G"));
    return shortcuts;
}

[[nodiscard]] std::vector<EditorShortcutBinding> AdvancedShortcuts()
{
    auto shortcuts = StandardShortcuts();
    shortcuts.push_back(
        Shortcut(kCtrl | kShift, 'B', "saga.command.build", "Ctrl+Shift+B"));
    shortcuts.push_back(
        Shortcut(kCtrl | kShift, 'D', "saga.command.view.diagnostics", "Ctrl+Shift+D"));
    return shortcuts;
}

[[nodiscard]] std::vector<std::string> CorePanels()
{
    return {
        "saga.panel.hierarchy",
        "saga.panel.viewport",
        "saga.panel.inspector",
    };
}

[[nodiscard]] std::vector<std::string> StandardToolbar()
{
    return {
        "saga.command.world.play",
        "saga.command.world.pause",
        "saga.command.world.stop",
        "saga.command.file.save",
        "saga.command.edit.undo",
        "saga.command.edit.redo",
    };
}

[[nodiscard]] std::vector<std::string> TransformTools()
{
    return {
        "saga.command.edit.mode.translate",
        "saga.command.edit.mode.rotate",
        "saga.command.edit.mode.scale",
    };
}

} // namespace

bool EditorProfile::operator==(const EditorProfile& other) const noexcept
{
    return id == other.id &&
           displayName == other.displayName &&
           description == other.description &&
           layoutPresetId == other.layoutPresetId &&
           shortcutMapId == other.shortcutMapId &&
           defaultPanels == other.defaultPanels &&
           defaultToolbarCommands == other.defaultToolbarCommands &&
           visibleToolCommands == other.visibleToolCommands &&
           shortcutBindings == other.shortcutBindings &&
           showMenuBar == other.showMenuBar &&
           showStatusBar == other.showStatusBar &&
           showMainToolbar == other.showMainToolbar &&
           exposesGraphEditor == other.exposesGraphEditor &&
           exposesProfiler == other.exposesProfiler &&
           exposesConsole == other.exposesConsole &&
           exposesAssetBrowser == other.exposesAssetBrowser;
}

EditorProfile MakeBasicProfile()
{
    EditorProfile p;
    p.id = "saga.profile.basic";
    p.displayName = "Basic";
    p.description = "Guided workspace with a small visible tool set.";
    p.layoutPresetId = "basic";
    p.shortcutMapId = "saga.shortcuts.basic";
    p.defaultPanels = {
        "saga.panel.production_dashboard",
        "saga.panel.hierarchy",
        "saga.panel.viewport",
        "saga.panel.console",
    };
    p.defaultToolbarCommands = {
        "saga.command.world.play",
        "saga.command.world.stop",
        "saga.command.file.save",
    };
    p.visibleToolCommands = {
        "saga.command.world.play",
        "saga.command.world.stop",
        "saga.command.file.save",
    };
    p.shortcutBindings = {
        Shortcut(kCtrl, 'S', "saga.command.file.save", "Ctrl+S"),
    };
    p.exposesProfiler = false;
    p.exposesAssetBrowser = false;
    return p;
}

EditorProfile MakeNodeEditorProfile()
{
    EditorProfile p;
    p.id = "saga.profile.node_editor";
    p.displayName = "Node Editor";
    p.description = "Node and graph focused workspace with script surfaces visible.";
    p.layoutPresetId = "node_editor";
    p.shortcutMapId = "saga.shortcuts.node_editor";
    p.defaultPanels = {
        "saga.panel.production_dashboard",
        "saga.panel.viewport",
        "saga.panel.graph",
        "saga.panel.console",
        "saga.panel.inspector",
    };
    p.defaultToolbarCommands = {
        "saga.command.world.play",
        "saga.command.world.stop",
        "saga.command.view.graph",
        "saga.command.file.save",
    };
    p.visibleToolCommands = {
        "saga.command.view.graph",
        "saga.command.edit.mode.translate",
    };
    p.shortcutBindings = NodeEditorShortcuts();
    p.exposesProfiler = false;
    p.exposesAssetBrowser = false;
    return p;
}

EditorProfile MakeStandardPipelineProfile()
{
    EditorProfile p;
    p.id = "saga.profile.standard_pipeline";
    p.displayName = "Standard Pipeline";
    p.description = "Balanced workspace for scene, asset, and script workflows.";
    p.layoutPresetId = "standard_pipeline";
    p.shortcutMapId = "saga.shortcuts.standard_pipeline";
    p.defaultPanels = CorePanels();
    p.defaultPanels.insert(p.defaultPanels.begin(), "saga.panel.production_dashboard");
    p.defaultPanels.push_back("saga.panel.console");
    p.defaultPanels.push_back("saga.panel.assetbrowser");
    p.defaultToolbarCommands = StandardToolbar();
    p.visibleToolCommands = TransformTools();
    p.shortcutBindings = StandardShortcuts();
    return p;
}

EditorProfile MakeAdvancedPipelineProfile()
{
    EditorProfile p;
    p.id = "saga.profile.advanced_pipeline";
    p.displayName = "Advanced Pipeline";
    p.description = "Dense production workspace with diagnostics and collaboration surfaces.";
    p.layoutPresetId = "advanced_pipeline";
    p.shortcutMapId = "saga.shortcuts.advanced_pipeline";
    p.defaultPanels = CorePanels();
    p.defaultPanels.insert(p.defaultPanels.begin(), "saga.panel.production_dashboard");
    p.defaultPanels.push_back("saga.panel.console");
    p.defaultPanels.push_back("saga.panel.assetbrowser");
    p.defaultPanels.push_back("saga.panel.profiler");
    p.defaultPanels.push_back("saga.panel.collaboration");
    p.defaultToolbarCommands = StandardToolbar();
    p.defaultToolbarCommands.push_back("saga.command.world.step");
    p.defaultToolbarCommands.push_back("saga.command.build");
    p.visibleToolCommands = TransformTools();
    p.visibleToolCommands.push_back("saga.command.build");
    p.visibleToolCommands.push_back("saga.command.view.diagnostics");
    p.shortcutBindings = AdvancedShortcuts();
    return p;
}

EditorProfile MakeCustomProfile()
{
    EditorProfile p = MakeStandardPipelineProfile();
    p.id = "saga.profile.custom";
    p.displayName = "Custom";
    p.description = "User-authored workspace profile stored in private editor settings.";
    p.layoutPresetId = "custom";
    p.shortcutMapId = "saga.shortcuts.custom";
    return p;
}

std::string DefaultEditorProfileId()
{
    return MakeBasicProfile().id;
}

std::string ResolveEditorProfileId(const std::string& id)
{
    if (id == "saga.profile.creator") return "saga.profile.basic";
    if (id == "saga.profile.explorer") return "saga.profile.node_editor";
    if (id == "saga.profile.builder") return "saga.profile.standard_pipeline";
    if (id == "saga.profile.studio") return "saga.profile.advanced_pipeline";
    return id;
}

} // namespace SagaEditor
