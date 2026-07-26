// SPDX-License-Identifier: MPL-2.0
/// @file main.cpp
/// @brief Provides the thin SagaEditor platform entry point.

#include "SagaEditorQt/EditorApplicationRunner.h"

#if SAGA_WITH_EDITORLAB_DEV_PANEL
#include "SagaEditorLabBridge.h"
#endif

#include <iostream>

int main(int argc, char* argv[])
{
#if SAGA_WITH_EDITORLAB_DEV_PANEL
    return SagaEditor::RunEditorApplication(
        argc, argv, std::cout, std::cerr,
        [](SagaEditor::EditorShell& shell)
        {
            SagaDev::InstallSagaEditorLabBridge(shell);
        });
#else
    return SagaEditor::RunEditorApplication(
        argc, argv, std::cout, std::cerr);
#endif
}
