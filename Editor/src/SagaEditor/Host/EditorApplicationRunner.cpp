/// @file EditorApplicationRunner.cpp
/// @brief Implements Editor-owned command parsing and application delegation.

#include "SagaEditor/Host/EditorApplicationRunner.h"

#include "SagaEditor/Host/EditorApp.h"
#include "SagaEditor/Host/EditorProjectInspection.h"

#include <cstdlib>
#include <filesystem>
#include <ostream>
#include <string>

namespace SagaEditor
{
namespace
{

constexpr int kExitStartupFailure = 1;
constexpr int kExitBadArguments = 2;
constexpr int kExitInspectionFailure = 3;

struct ParsedEditorCommand
{
    EditorAppConfig app;
    std::filesystem::path inspectionProject;
    std::filesystem::path inspectionReport;
    bool showHelp = false;
};

[[nodiscard]] bool HasValue(int index, int argc) noexcept
{
    return index + 1 < argc;
}

[[nodiscard]] bool ParsePositiveInt(const char* text, int& out) noexcept
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

[[nodiscard]] bool ParseEditorCommand(int argc,
                                      char* argv[],
                                      ParsedEditorCommand& parsed,
                                      std::ostream& err)
{
    parsed.app.argc = argc;
    parsed.app.argv = argv;
    parsed.app.windowTitle = "SagaEditor";
    parsed.app.maximized = true;

    for (int i = 1; i < argc; ++i)
    {
        const std::string argument = argv[i] ? argv[i] : "";
        auto requireValue = [&](const char* option) -> const char*
        {
            if (!HasValue(i, argc))
            {
                err << "SagaEditor: " << option << " requires a value\n";
                return nullptr;
            }
            return argv[++i];
        };

        if (argument == "--help" || argument == "--?")
        {
            parsed.showHelp = true;
        }
        else if (argument == "--workspace")
        {
            const char* value = requireValue("--workspace");
            if (value == nullptr) return false;
            parsed.app.workspacePath = value;
        }
        else if (argument == "--profile")
        {
            const char* value = requireValue("--profile");
            if (value == nullptr) return false;
            parsed.app.initialProfileId = value;
        }
        else if (argument == "--composition-manifest")
        {
            const char* value = requireValue("--composition-manifest");
            if (value == nullptr) return false;
            parsed.app.composition.manifestPath = value;
        }
        else if (argument == "--composition-overlay")
        {
            const char* value = requireValue("--composition-overlay");
            if (value == nullptr) return false;
            parsed.app.composition.overlayPaths.emplace_back(value);
        }
        else if (argument == "--require-composition")
        {
            parsed.app.composition.requireValidComposition = true;
        }
        else if (argument == "--smoke")
        {
            parsed.app.smoke = true;
            parsed.app.showOnInit = false;
        }
        else if (argument == "--smoke-timeout-ms")
        {
            const char* value = requireValue("--smoke-timeout-ms");
            if (value == nullptr ||
                !ParsePositiveInt(value, parsed.app.smokeTimeoutMs))
            {
                err << "SagaEditor: --smoke-timeout-ms requires a positive integer\n";
                return false;
            }
        }
        else if (argument == "--no-show")
        {
            parsed.app.showOnInit = false;
        }
        else if (argument == "--windowed")
        {
            parsed.app.maximized = false;
        }
        else if (argument == "--width" || argument == "--height")
        {
            const char* value = requireValue(argument.c_str());
            int& destination = argument == "--width" ?
                parsed.app.windowWidth : parsed.app.windowHeight;
            if (value == nullptr || !ParsePositiveInt(value, destination))
            {
                err << "SagaEditor: " << argument
                    << " requires a positive integer\n";
                return false;
            }
        }
        else if (argument == "--inspect-project")
        {
            const char* value = requireValue("--inspect-project");
            if (value == nullptr) return false;
            parsed.inspectionProject = value;
        }
        else if (argument == "--editor-shell-report")
        {
            const char* value = requireValue("--editor-shell-report");
            if (value == nullptr) return false;
            parsed.inspectionReport = value;
        }
        else
        {
            err << "SagaEditor: unknown argument '" << argument << "'\n";
            return false;
        }
    }

    if (parsed.inspectionProject.empty() != parsed.inspectionReport.empty())
    {
        err << "SagaEditor: --inspect-project and --editor-shell-report "
               "must be used together\n";
        return false;
    }
    return true;
}

} // namespace

const char* EditorApplicationUsage() noexcept
{
    return
        "Usage: SagaEditor [options]\n"
        "  --workspace <path>             Open an Editor workspace\n"
        "  --profile <id>                 Select a bounded Editor profile\n"
        "  --inspect-project <path>       Inspect a project read model\n"
        "  --editor-shell-report <path>   Write schema-2 inspection JSON\n"
        "  --smoke                        Run bounded headless startup smoke\n"
        "  --smoke-timeout-ms <n>         Bound startup smoke duration\n"
        "  --no-show                      Initialize without showing a window\n"
        "  --windowed                     Do not maximize the Editor window\n"
        "  --width <n> --height <n>       Set initial window dimensions\n"
        "  --help, --?                    Show this help\n";
}

int RunEditorApplication(int argc,
                         char* argv[],
                         IUIFactory& factory,
                         std::ostream& out,
                         std::ostream& err,
                         EditorShellExtension extension)
{
    ParsedEditorCommand parsed;
    if (!ParseEditorCommand(argc, argv, parsed, err))
    {
        return kExitBadArguments;
    }
    if (parsed.showHelp)
    {
        out << EditorApplicationUsage();
        return 0;
    }
    if (!parsed.inspectionProject.empty())
    {
        EditorProjectInspectionRequest request;
        request.projectPath = parsed.inspectionProject;
        request.reportPath = parsed.inspectionReport;
        request.requestedProfileId = parsed.app.initialProfileId;
        const EditorProjectInspectionResult result = InspectEditorProject(request);
        if (!result.error.empty())
        {
            err << "SagaEditor: " << result.error << '\n';
        }
        if (!result.reportPath.empty())
        {
            out << "SagaEditor: editor shell report="
                << result.reportPath << '\n';
        }
        return result.ok ? 0 : kExitInspectionFailure;
    }

    EditorApp app;
    if (!app.Init(parsed.app, factory))
    {
        err << "SagaEditor: startup failed\n";
        return kExitStartupFailure;
    }
    if (extension)
    {
        extension(app.GetShell());
    }
    const int exitCode = parsed.app.smoke ?
        app.RunSmoke(parsed.app.smokeTimeoutMs) : app.Run();
    app.Shutdown();
    return exitCode;
}

} // namespace SagaEditor
