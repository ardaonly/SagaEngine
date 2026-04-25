/// @file ShellCommands.cpp
/// @brief Registration of built-in shell commands (File, Edit, View, World).

#include "SagaEditor/Shell/ShellCommands.h"
#include "SagaEditor/Commands/CommandRegistry.h"
#include "SagaEditor/Commands/UndoRedoStack.h"

namespace SagaEditor
{

// ─── Registration ─────────────────────────────────────────────────────────────

void RegisterShellCommands(CommandRegistry& registry, UndoRedoStack& undoStack)
{
    // File
    registry.Register({ "saga.file.newProject",  "New Project",    "File" });
    registry.Register({ "saga.file.openProject", "Open Project",   "File" });
    registry.Register({ "saga.file.saveProject", "Save Project",   "File" });
    registry.Register({ "saga.file.exit",        "Exit",           "File" });

    // Edit — undo/redo delegates to UndoRedoStack
    registry.Register({ "saga.edit.undo",        "Undo",           "Edit",
        [&undoStack]() { undoStack.Undo(); } });
    registry.Register({ "saga.edit.redo",        "Redo",           "Edit",
        [&undoStack]() { undoStack.Redo(); } });
    registry.Register({ "saga.edit.preferences", "Preferences",    "Edit" });

    // View — toggling panels is wired per-panel after shell init
    registry.Register({ "saga.view.hierarchy",      "Hierarchy",      "View" });
    registry.Register({ "saga.view.inspector",      "Inspector",      "View" });
    registry.Register({ "saga.view.assetBrowser",   "Asset Browser",  "View" });
    registry.Register({ "saga.view.console",        "Console",        "View" });
    registry.Register({ "saga.view.commandPalette", "Command Palette","View" });

    // World (MMO-specific)
    registry.Register({ "saga.world.settings",    "World Settings",     "World" });
    registry.Register({ "saga.world.zoneManager", "Zone Manager",       "World" });
    registry.Register({ "saga.world.cellDebug",   "Cell Debug Overlay", "World" });
}

} // namespace SagaEditor
