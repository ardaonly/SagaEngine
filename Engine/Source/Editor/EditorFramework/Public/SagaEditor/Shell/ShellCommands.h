/// @file ShellCommands.h
/// @brief Free function that registers all built-in shell commands.

#pragma once

namespace SagaEditor
{

class CommandRegistry;
class UndoRedoStack;

/// Register File, Edit, View, and World commands into the command registry.
/// Called by EditorHost::Init after the registry and undo stack are ready.
void RegisterShellCommands(CommandRegistry& registry, UndoRedoStack& undoStack);

} // namespace SagaEditor
