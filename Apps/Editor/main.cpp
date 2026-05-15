/// @file main.cpp
/// @brief SagaEditor platform entry point.
///
/// Qt handles the WinMain shim on Windows via qt_add_executable(WIN32) in
/// SagaTargets.cmake — no separate WinMain wrapper is needed.

#include "SagaEditor/Host/EditorApp.h"
#include "SagaEditor/UI/Qt/QtUIFactory.h"

#include <cstdlib>
#include <iostream>
#include <string>

namespace
{

constexpr int kExitStartupFailure = 1;
constexpr int kExitBadArguments = 2;

[[nodiscard]] bool HasValue(int index, int argc) noexcept
{
    return index + 1 < argc;
}

[[nodiscard]] bool ParseInt(const char* text, int& out) noexcept
{
    char* end = nullptr;
    const long value = std::strtol(text, &end, 10);
    if (end == text || *end != '\0' || value <= 0)
    {
        return false;
    }
    out = static_cast<int>(value);
    return true;
}

[[nodiscard]] bool BuildEditorConfig(int argc,
                                     char* argv[],
                                     SagaEditor::EditorAppConfig& cfg)
{
    cfg.argc = argc;
    cfg.argv = argv;
    cfg.windowTitle = "SagaEditor";
    cfg.maximized = true;

    for (int i = 1; i < argc; ++i)
    {
        const std::string arg = argv[i] ? argv[i] : "";
        if (arg == "--workspace")
        {
            if (!HasValue(i, argc))
            {
                std::cerr << "SagaEditor: --workspace requires a path\n";
                return false;
            }
            cfg.workspacePath = argv[++i];
        }
        else if (arg == "--profile")
        {
            if (!HasValue(i, argc))
            {
                std::cerr << "SagaEditor: --profile requires a profile id\n";
                return false;
            }
            cfg.initialProfileId = argv[++i];
        }
        else if (arg == "--windowed")
        {
            cfg.maximized = false;
        }
        else if (arg == "--width")
        {
            if (!HasValue(i, argc) || !ParseInt(argv[++i], cfg.windowWidth))
            {
                std::cerr << "SagaEditor: --width requires a positive integer\n";
                return false;
            }
        }
        else if (arg == "--height")
        {
            if (!HasValue(i, argc) || !ParseInt(argv[++i], cfg.windowHeight))
            {
                std::cerr << "SagaEditor: --height requires a positive integer\n";
                return false;
            }
        }
        else
        {
            std::cerr << "SagaEditor: unknown argument '" << arg << "'\n";
            return false;
        }
    }

    return true;
}

} // namespace

int main(int argc, char* argv[])
{
    SagaEditor::QtUIFactory factory;

    SagaEditor::EditorAppConfig cfg;
    if (!BuildEditorConfig(argc, argv, cfg))
    {
        return kExitBadArguments;
    }

    SagaEditor::EditorApp app;

    if (!app.Init(cfg, factory))
        return kExitStartupFailure;

    const int exitCode = app.Run();
    app.Shutdown();
    return exitCode;
}
