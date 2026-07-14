/// @file SagaEditorLabBridge.h
/// @brief Dev-only bridge that mounts EditorLab panels into SagaEditor.

#pragma once

namespace SagaEditor
{
class EditorShell;
}

namespace SagaDev
{

/// Install EditorLab panels into an initialized Editor shell.
void InstallSagaEditorLabBridge(SagaEditor::EditorShell& shell);

} // namespace SagaDev
