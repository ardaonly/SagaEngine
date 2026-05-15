/// @file ShellCommands.cpp
/// @brief Registration of built-in shell commands (File, Edit, View, World).

#include "SagaEditor/Shell/ShellCommands.h"
#include "SagaEditor/Commands/CommandRegistry.h"
#include "SagaEditor/Commands/UndoRedoStack.h"

#include <string>
#include <utility>

namespace SagaEditor
{

namespace
{

void RegisterStub(CommandRegistry& registry,
                  std::string      id,
                  std::string      label,
                  std::string      category)
{
    registry.Register({
        std::move(id),
        std::move(label),
        std::move(category),
        {},
        true
    });
}

} // namespace

// ─── Registration ─────────────────────────────────────────────────────────────

void RegisterShellCommands(CommandRegistry& registry, UndoRedoStack& undoStack)
{
    // File
    RegisterStub(registry, "saga.command.file.new_scene",    "New Scene",    "File");
    RegisterStub(registry, "saga.command.file.open_project", "Open Project", "File");
    RegisterStub(registry, "saga.command.file.save",         "Save Scene",   "File");
    RegisterStub(registry, "saga.command.file.exit",         "Exit",         "File");

    // Edit — undo/redo delegates to UndoRedoStack
    registry.Register({ "saga.command.edit.undo",        "Undo",           "Edit",
        [&undoStack]() { undoStack.Undo(); } });
    registry.Register({ "saga.command.edit.redo",        "Redo",           "Edit",
        [&undoStack]() { undoStack.Redo(); } });
    RegisterStub(registry, "saga.command.edit.preferences", "Preferences", "Edit");

    // View — toggling panels is wired per-panel after shell init
    RegisterStub(registry, "saga.command.view.hierarchy",       "Hierarchy",       "View");
    RegisterStub(registry, "saga.command.view.inspector",       "Inspector",       "View");
    RegisterStub(registry, "saga.command.view.asset_browser",   "Asset Browser",   "View");
    RegisterStub(registry, "saga.command.view.console",         "Console",         "View");
    RegisterStub(registry, "saga.command.view.production_dashboard", "Production Dashboard", "View");
    RegisterStub(registry, "saga.command.view.command_palette", "Command Palette", "View");
    RegisterStub(registry, "saga.command.view.theme",           "Theme",           "View");
    RegisterStub(registry, "saga.command.view.graph",           "Graph",           "View");
    RegisterStub(registry, "saga.command.view.diagnostics",     "Diagnostics",     "View");

    // World (MMO-specific)
    RegisterStub(registry, "saga.command.world.add_entity",    "Add Entity",         "World");
    RegisterStub(registry, "saga.command.world.bake_lighting", "Bake Lighting",      "World");
    RegisterStub(registry, "saga.command.world.settings",      "World Settings",     "World");
    RegisterStub(registry, "saga.command.world.play",          "Play",               "World");
    RegisterStub(registry, "saga.command.world.pause",         "Pause",              "World");
    RegisterStub(registry, "saga.command.world.stop",          "Stop",               "World");
    RegisterStub(registry, "saga.command.world.step",          "Step",               "World");
    RegisterStub(registry, "saga.command.world.zone_manager",  "Zone Manager",       "World");
    RegisterStub(registry, "saga.command.world.cell_debug",    "Cell Debug Overlay", "World");
    RegisterStub(registry, "saga.command.build",               "Build",              "World");
    RegisterStub(registry, "saga.command.edit.mode.translate", "Translate Mode",     "World");
    RegisterStub(registry, "saga.command.edit.mode.rotate",    "Rotate Mode",        "World");
    RegisterStub(registry, "saga.command.edit.mode.scale",     "Scale Mode",         "World");
}

} // namespace SagaEditor
