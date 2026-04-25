/// @file main.cpp
/// @brief SagaEditor platform entry point.
///
/// Qt handles the WinMain shim on Windows via qt_add_executable(WIN32) in
/// SagaTargets.cmake — no separate WinMain wrapper is needed.

#include "SagaEditor/Host/EditorApp.h"
#include "SagaEditor/UI/Qt/QtUIFactory.h"

int main(int argc, char* argv[])
{
    SagaEditor::QtUIFactory factory;

    SagaEditor::EditorAppConfig cfg;
    cfg.argc        = argc;
    cfg.argv        = argv;
    cfg.maximized   = true;

    SagaEditor::EditorApp app;

    if (!app.Init(cfg, factory))
        return 1;

    const int exitCode = app.Run();
    app.Shutdown();
    return exitCode;
}
