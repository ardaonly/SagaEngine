/// @file main.cpp
/// @brief Provides the thin SagaEditor platform entry point.

#include "SagaEditor/Host/EditorApplicationRunner.h"
#include "SagaEditorQt/QtUIFactory.h"

#if SAGA_WITH_EDITORLAB_DEV_PANEL
#include "SagaEditorLabBridge.h"
#endif

#include <iostream>

int main(int argc, char* argv[])
{
    SagaEditor::QtUIFactory factory;
#if SAGA_WITH_EDITORLAB_DEV_PANEL
    return SagaEditor::RunEditorApplication(
        argc, argv, factory, std::cout, std::cerr,
        [](SagaEditor::EditorShell& shell)
        {
            SagaDev::InstallSagaEditorLabBridge(shell);
        });
#else
    return SagaEditor::RunEditorApplication(
        argc, argv, factory, std::cout, std::cerr);
#endif
}
