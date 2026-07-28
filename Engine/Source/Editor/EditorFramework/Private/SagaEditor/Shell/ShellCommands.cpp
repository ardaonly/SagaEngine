// SPDX-License-Identifier: MPL-2.0
/// @file ShellCommands.cpp
/// @brief Registration of built-in shell commands (File, Edit, View, World).

#include "SagaEditor/Shell/ShellCommands.h"
#include "SagaEditor/Commands/CommandRegistry.h"
#include "SagaEditor/Commands/UndoRedoStack.h"

#include <string>
#include <utility>

namespace SagaEditor
{

// ─── Registration ─────────────────────────────────────────────────────────────

void RegisterShellCommands(CommandRegistry& registry, UndoRedoStack& undoStack)
{
    // Edit — undo/redo delegates to UndoRedoStack
    registry.Register({ "saga.command.edit.undo",        "Undo",           "Edit",
        [&undoStack]() { undoStack.Undo(); } });
    registry.Register({ "saga.command.edit.redo",        "Redo",           "Edit",
        [&undoStack]() { undoStack.Redo(); } });
}

} // namespace SagaEditor
