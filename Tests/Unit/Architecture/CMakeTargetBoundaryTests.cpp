/// @file CMakeTargetBoundaryTests.cpp
/// @brief Guards critical CMake target dependency boundaries.

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{

struct CMakeLinkCall
{
    std::string target; ///< First argument passed to target_link_libraries.
    std::string text;   ///< Full call text collected from SagaTargets.cmake.
    std::size_t line = 0; ///< 1-based source line for failure diagnostics.
};

std::string ReadText(const std::filesystem::path& path)
{
    std::ifstream input(path);
    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

std::vector<std::string> ReadLines(const std::filesystem::path& path)
{
    std::ifstream input(path);
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(input, line))
    {
        lines.push_back(line);
    }
    return lines;
}

std::string Trim(std::string_view value)
{
    const auto begin = value.find_first_not_of(" \t\r\n");
    if (begin == std::string_view::npos)
    {
        return {};
    }

    const auto end = value.find_last_not_of(" \t\r\n");
    return std::string(value.substr(begin, end - begin + 1));
}

bool StartsWith(std::string_view value, std::string_view prefix)
{
    return value.size() >= prefix.size() &&
           value.substr(0, prefix.size()) == prefix;
}

std::string ExtractFirstArgument(const std::string& line)
{
    constexpr std::string_view marker = "target_link_libraries(";
    const auto start = line.find(marker);
    if (start == std::string::npos)
    {
        return {};
    }

    auto cursor = start + marker.size();
    while (cursor < line.size() &&
           (line[cursor] == ' ' || line[cursor] == '\t'))
    {
        ++cursor;
    }

    const auto end = line.find_first_of(" \t\r\n)", cursor);
    if (end == std::string::npos)
    {
        return line.substr(cursor);
    }

    return line.substr(cursor, end - cursor);
}

std::vector<CMakeLinkCall> ExtractTargetLinkCalls(
    const std::vector<std::string>& lines)
{
    std::vector<CMakeLinkCall> calls;

    for (std::size_t i = 0; i < lines.size(); ++i)
    {
        const auto& line = lines[i];
        if (line.find("target_link_libraries(") == std::string::npos)
        {
            continue;
        }

        CMakeLinkCall call;
        call.target = ExtractFirstArgument(line);
        call.line = i + 1;
        call.text = line + "\n";

        while (call.text.find(')') == std::string::npos &&
               i + 1 < lines.size())
        {
            ++i;
            call.text += lines[i] + "\n";
        }

        calls.push_back(std::move(call));
    }

    return calls;
}

bool ContainsToken(const std::string& text, const std::string& token)
{
    return text.find(token) != std::string::npos;
}

bool IsCodeOrBuildFile(const std::filesystem::path& path)
{
    const auto filename = path.filename().string();
    const auto extension = path.extension().string();
    return filename == "CMakeLists.txt" ||
           extension == ".h" ||
           extension == ".hpp" ||
           extension == ".cpp" ||
           extension == ".rs" ||
           extension == ".py";
}

std::vector<std::string> FindForbiddenText(
    const std::filesystem::path& root,
    const std::vector<std::string>& forbiddenTokens)
{
    std::vector<std::string> offenders;
    if (!std::filesystem::exists(root))
    {
        return offenders;
    }

    for (const auto& entry : std::filesystem::recursive_directory_iterator(root))
    {
        if (!entry.is_regular_file() || !IsCodeOrBuildFile(entry.path()))
        {
            continue;
        }

        const std::string text = ReadText(entry.path());
        for (const std::string& token : forbiddenTokens)
        {
            if (text.find(token) != std::string::npos)
            {
                offenders.push_back(
                    entry.path().generic_string() + ": " + token);
                break;
            }
        }
    }
    return offenders;
}

std::vector<CMakeLinkCall> FindForbiddenLinks(
    const std::vector<CMakeLinkCall>& calls,
    const std::string& target,
    const std::vector<std::string>& forbiddenDependencies)
{
    std::vector<CMakeLinkCall> offenders;
    for (const auto& call : calls)
    {
        if (call.target != target)
        {
            continue;
        }

        for (const auto& dependency : forbiddenDependencies)
        {
            if (ContainsToken(call.text, dependency))
            {
                offenders.push_back(call);
                break;
            }
        }
    }

    return offenders;
}

std::vector<std::string> FindForbiddenDependencyNames(
    const CMakeLinkCall& call,
    const std::vector<std::string>& forbiddenDependencies)
{
    std::vector<std::string> names;
    for (const auto& dependency : forbiddenDependencies)
    {
        if (ContainsToken(call.text, dependency))
        {
            names.push_back(dependency);
        }
    }
    return names;
}

std::string JoinNames(const std::vector<std::string>& names)
{
    std::string result;
    for (std::size_t i = 0; i < names.size(); ++i)
    {
        if (i != 0)
        {
            result += ", ";
        }
        result += names[i];
    }
    return result;
}

bool IsLineInsideIfGuard(
    const std::vector<std::string>& lines,
    std::size_t lineIndex,
    const std::string& guardName)
{
    std::vector<std::string> guardStack;
    for (std::size_t i = 0; i <= lineIndex && i < lines.size(); ++i)
    {
        const std::string trimmed = Trim(lines[i]);
        if (StartsWith(trimmed, "if("))
        {
            guardStack.push_back(trimmed);
            continue;
        }

        if (StartsWith(trimmed, "endif"))
        {
            if (!guardStack.empty())
            {
                guardStack.pop_back();
            }
        }
    }

    for (const auto& guard : guardStack)
    {
        if (guard.find(guardName) != std::string::npos)
        {
            return true;
        }
    }

    return false;
}

std::filesystem::path SagaTargetsPath()
{
    return std::filesystem::path(SAGA_SOURCE_ROOT) /
           "cmake" / "modules" / "SagaTargets.cmake";
}

std::filesystem::path CMakeModulePath(std::string_view filename)
{
    return std::filesystem::path(SAGA_SOURCE_ROOT) /
           "cmake" / "modules" / std::string(filename);
}

} // namespace

TEST(CMakeTargetBoundaryTests, SagaProductLibDoesNotLinkEditorLabTargets)
{
    const auto path = SagaTargetsPath();
    const auto calls = ExtractTargetLinkCalls(ReadLines(path));
    const std::vector<std::string> forbidden = {
        "SagaEditorLabLib",
        "SagaEditorLabBridge",
    };

    const auto offenders =
        FindForbiddenLinks(calls, "SagaProductLib", forbidden);

    for (const auto& offender : offenders)
    {
        ADD_FAILURE()
            << "Forbidden target dependency: SagaProductLib must not link "
            << JoinNames(FindForbiddenDependencyNames(offender, forbidden))
            << ". Offending call in " << path.generic_string()
            << ":" << offender.line << "\n" << offender.text;
    }
}

TEST(CMakeTargetBoundaryTests, SagaAssetPipelineLibDoesNotLinkRuntimeEditorProductOrToolOwners)
{
    const auto path = SagaTargetsPath();
    const auto calls = ExtractTargetLinkCalls(ReadLines(path));
    const std::vector<std::string> forbidden = {
        "SagaRuntimeLib",
        "SagaEditorLib",
        "SagaProductLib",
        "Forge",
        "Prism",
        "SDE",
    };

    const auto offenders =
        FindForbiddenLinks(calls, "SagaAssetPipelineLib", forbidden);

    for (const auto& offender : offenders)
    {
        ADD_FAILURE()
            << "Forbidden target dependency: SagaAssetPipelineLib must not link "
            << JoinNames(FindForbiddenDependencyNames(offender, forbidden))
            << ". Offending call in " << path.generic_string()
            << ":" << offender.line << "\n" << offender.text;
    }
}

TEST(CMakeTargetBoundaryTests, SagaEngineDoesNotLinkProductEditorOrToolTargets)
{
    const auto path = SagaTargetsPath();
    const auto calls = ExtractTargetLinkCalls(ReadLines(path));
    const std::vector<std::string> forbidden = {
        "SagaProductLib",
        "SagaEditorLib",
        "SagaEditorLabLib",
        "SagaEditorLabBridge",
        "SagaAssetPipelineLib",
        "SagaPipelineLib",
        "SagaEditorCompositionLib",
        "Forge",
        "Prism",
        "SDE",
    };

    const auto offenders =
        FindForbiddenLinks(calls, "SagaEngine", forbidden);

    for (const auto& offender : offenders)
    {
        ADD_FAILURE()
            << "Forbidden target dependency: SagaEngine must not link "
            << JoinNames(FindForbiddenDependencyNames(offender, forbidden))
            << ". Offending call in " << path.generic_string()
            << ":" << offender.line << "\n" << offender.text;
    }
}

TEST(CMakeTargetBoundaryTests, SagaGraphicsTargetIsVendorNeutralPublicShell)
{
    const auto path = SagaTargetsPath();
    const auto text = ReadText(path);

    EXPECT_TRUE(ContainsToken(text, "add_library(SagaGraphics INTERFACE)"))
        << "SagaGraphics must exist as a public interface target.";
    EXPECT_TRUE(ContainsToken(
        text,
        "target_include_directories(SagaGraphics INTERFACE\n"
        "        ${SAGA_ROOT}/Engine/Public"))
        << "SagaGraphics must expose only the Engine/Public include root.";
    EXPECT_FALSE(ContainsToken(text, "SagaGraphicsPrivate"))
        << "SagaGraphicsPrivate is out of scope until private graphics "
           "implementation exists.";
    EXPECT_FALSE(ContainsToken(text, "target_link_libraries(SagaGraphics"))
        << "SagaGraphics must not link vendor, backend, native API, or "
           "backend library targets.";
}

TEST(CMakeTargetBoundaryTests, GraphicsInstallSurfaceDoesNotInstallVendorBackends)
{
    const auto installPath = CMakeModulePath("SagaInstall.cmake");
    const auto vendorPath = CMakeModulePath("SagaDiligentVendor.cmake");
    ASSERT_TRUE(std::filesystem::exists(installPath));
    ASSERT_TRUE(std::filesystem::exists(vendorPath));

    const auto installText = ReadText(installPath);
    EXPECT_TRUE(ContainsToken(
        installText,
        "install(DIRECTORY \"${SAGA_ROOT}/Engine/Public/SagaEngine\""))
        << "SagaEngine public include tree must remain the installed "
           "development include surface.";
    EXPECT_FALSE(ContainsToken(installText, "Vendor/Diligent"))
        << "Install rules must not copy vendored graphics sources.";
    EXPECT_FALSE(ContainsToken(installText, "VendorDiligent"))
        << "VendorDiligent must not be installed as a public target.";
    EXPECT_FALSE(ContainsToken(installText, "SagaDiligentBackend"))
        << "Concrete Diligent backend target must not be installed.";

    const auto vendorText = ReadText(vendorPath);
    EXPECT_TRUE(ContainsToken(vendorText, "DILIGENT_INSTALL_CORE OFF"));
    EXPECT_TRUE(ContainsToken(vendorText, "DILIGENT_INSTALL_FX OFF"));
    EXPECT_TRUE(ContainsToken(vendorText, "DILIGENT_INSTALL_TOOLS OFF"));
    EXPECT_TRUE(ContainsToken(vendorText, "DILIGENT_INSTALL_SAMPLES OFF"));
}

TEST(CMakeTargetBoundaryTests, SagaDiagnosticsDoesNotLinkRuntimeServerEditorOrToolTargets)
{
    const auto path = SagaTargetsPath();
    const auto calls = ExtractTargetLinkCalls(ReadLines(path));
    const std::vector<std::string> forbidden = {
        "SagaRuntimeLib",
        "SagaServerLib",
        "SagaProductLib",
        "SagaEditorLib",
        "SagaEditorLabLib",
        "SagaEditorLabBridge",
        "SagaAssetPipelineLib",
        "SagaPipelineLib",
        "SagaEditorCompositionLib",
        "Forge",
        "Prism",
        "SDE",
    };

    const auto offenders =
        FindForbiddenLinks(calls, "SagaDiagnostics", forbidden);

    for (const auto& offender : offenders)
    {
        ADD_FAILURE()
            << "Forbidden target dependency: SagaDiagnostics must not link "
            << JoinNames(FindForbiddenDependencyNames(offender, forbidden))
            << ". Offending call in " << path.generic_string()
            << ":" << offender.line << "\n" << offender.text;
    }
}

TEST(CMakeTargetBoundaryTests, RuntimeAndServerTargetsDoNotLinkEditorDevOrToolTargets)
{
    const auto path = SagaTargetsPath();
    const auto calls = ExtractTargetLinkCalls(ReadLines(path));
    const std::vector<std::string> runtimeTargets = {
        "SagaRuntimeLib",
        "SagaServerLib",
        "SagaRuntime",
        "SagaServer",
    };
    const std::vector<std::string> forbidden = {
        "SagaEditorLib",
        "SagaEditorLabLib",
        "SagaEditorLabBridge",
        "SagaProductLib",
        "SagaAssetPipelineLib",
        "Forge",
        "Prism",
        "SDE",
    };

    for (const auto& target : runtimeTargets)
    {
        const auto offenders = FindForbiddenLinks(calls, target, forbidden);
        for (const auto& offender : offenders)
        {
            ADD_FAILURE()
                << "Forbidden target dependency: " << target
                << " must not link "
                << JoinNames(FindForbiddenDependencyNames(offender, forbidden))
                << ". Offending call in " << path.generic_string()
                << ":" << offender.line << "\n" << offender.text;
        }
    }
}

TEST(CMakeTargetBoundaryTests, SagaEnginePublicHeadersDoNotExposeSdeTypes)
{
    const auto root = std::filesystem::path(SAGA_SOURCE_ROOT);
    const auto offenders = FindForbiddenText(
        root / "Engine" / "Public",
        {
            "#include \"SDE/",
            "#include <SDE/",
            "SDE::",
        });

    EXPECT_TRUE(offenders.empty())
        << "SagaEngine public headers must not include or expose SDE types. "
        << "First offender: "
        << (offenders.empty() ? "" : offenders.front());
}

TEST(CMakeTargetBoundaryTests, RmlUiIsLinkedOnlyBySagaBackend)
{
    const auto path = SagaTargetsPath();
    const auto lines = ReadLines(path);

    bool sawBackendLink = false;
    std::vector<std::string> offenders;
    for (std::size_t i = 0; i < lines.size(); ++i)
    {
        const auto& line = lines[i];
        if (line.find("saga_link_rmlui(") == std::string::npos)
        {
            continue;
        }

        if (line.find("SagaBackend") != std::string::npos)
        {
            sawBackendLink = true;
            continue;
        }

        offenders.push_back(
            path.generic_string() + ":" + std::to_string(i + 1) + ": " + line);
    }

    EXPECT_TRUE(sawBackendLink)
        << "SagaBackend must own the RmlUi adapter link dependency.";
    EXPECT_TRUE(offenders.empty())
        << "RmlUi must not be linked outside SagaBackend. First offender: "
        << (offenders.empty() ? "" : offenders.front());
}

TEST(CMakeTargetBoundaryTests, EditorLabBridgeIsGuardedByDevPanelFlag)
{
    const auto path = SagaTargetsPath();
    const auto lines = ReadLines(path);
    constexpr const char* guardName = "SAGA_WITH_EDITORLAB_DEV_PANEL";

    bool sawBridgeTarget = false;
    for (std::size_t i = 0; i < lines.size(); ++i)
    {
        const bool mentionsBridge =
            lines[i].find("SagaEditorLabBridge") != std::string::npos;
        if (!mentionsBridge)
        {
            continue;
        }

        sawBridgeTarget = true;
        EXPECT_TRUE(IsLineInsideIfGuard(lines, i, guardName))
            << "SagaEditorLabBridge must only appear inside if("
            << guardName << "). Offending line in " << path.generic_string()
            << ":" << (i + 1) << ": " << lines[i];
    }

    EXPECT_TRUE(sawBridgeTarget)
        << "Expected SagaEditorLabBridge dev-only bridge target in "
        << path.generic_string();
}

TEST(CMakeTargetBoundaryTests, SagaLinksEditorLabBridgeOnlyBehindDevPanelFlag)
{
    const auto path = SagaTargetsPath();
    const auto lines = ReadLines(path);
    const auto calls = ExtractTargetLinkCalls(lines);
    constexpr const char* guardName = "SAGA_WITH_EDITORLAB_DEV_PANEL";

    bool sawSagaBridgeLink = false;
    for (const auto& call : calls)
    {
        if (call.target != "Saga" ||
            !ContainsToken(call.text, "SagaEditorLabBridge"))
        {
            continue;
        }

        sawSagaBridgeLink = true;
        ASSERT_GT(call.line, 0u);
        EXPECT_TRUE(IsLineInsideIfGuard(lines, call.line - 1, guardName))
            << "Forbidden unguarded dependency: Saga may link "
            << "SagaEditorLabBridge only inside if(" << guardName
            << "). Offending call in " << path.generic_string()
            << ":" << call.line << "\n" << call.text;
    }

    EXPECT_TRUE(sawSagaBridgeLink)
        << "Expected Saga executable to link SagaEditorLabBridge behind if("
        << guardName << ") in " << path.generic_string();
}

TEST(CMakeTargetBoundaryTests, ForgeDoesNotOwnSagaEditorCompositionWorkflow)
{
    const auto root = std::filesystem::path(SAGA_SOURCE_ROOT);
    const auto offenders = FindForbiddenText(
        root / "Tools" / "Forge",
        {
            "EditorCompositionOrchestrator",
            "saga-editor-composition-compiler",
            "Editor/CompositionSources",
            "saga.editor.default",
        });

    EXPECT_TRUE(offenders.empty())
        << "Forge must stay generic and not own SagaEditor composition workflow. "
        << "First offender: "
        << (offenders.empty() ? "" : offenders.front());
}

TEST(CMakeTargetBoundaryTests, GenericToolsAndAppsDoNotIncludeSagaPipeline)
{
    const auto root = std::filesystem::path(SAGA_SOURCE_ROOT);
    std::vector<std::string> offenders;
    for (const auto& directory : {
             root / "Tools" / "SystemDefinitionEngine",
             root / "Tools" / "Prism",
             root / "Apps" / "Editor",
         })
    {
        const auto hits = FindForbiddenText(directory, {"SagaPipeline/"});
        offenders.insert(offenders.end(), hits.begin(), hits.end());
    }

    EXPECT_TRUE(offenders.empty())
        << "SDE, Prism, and Apps/Editor must not include SagaPipeline internals. "
        << "First offender: "
        << (offenders.empty() ? "" : offenders.front());
}

TEST(CMakeTargetBoundaryTests, EditorCustomizationCoreStaysQtAndToolFree)
{
    const auto root = std::filesystem::path(SAGA_SOURCE_ROOT);
    const std::vector<std::filesystem::path> files = {
        root / "Editor" / "include" / "SagaEditor" / "Customization" / "EditorCustomizationCapability.h",
        root / "Editor" / "include" / "SagaEditor" / "Customization" / "EditorCustomizationDiagnostics.h",
        root / "Editor" / "include" / "SagaEditor" / "Customization" / "EditorCustomizationOverlay.h",
        root / "Editor" / "include" / "SagaEditor" / "Customization" / "EditorCustomizationOverlayLoader.h",
        root / "Editor" / "include" / "SagaEditor" / "Customization" / "ShortcutCustomizationController.h",
        root / "Editor" / "include" / "SagaEditor" / "Customization" / "ShortcutCustomizationFeedback.h",
        root / "Editor" / "include" / "SagaEditor" / "Customization" / "ShortcutCustomizationModel.h",
        root / "Editor" / "include" / "SagaEditor" / "Customization" / "ShortcutCustomizationPanelViewModel.h",
        root / "Editor" / "include" / "SagaEditor" / "Customization" / "ShortcutCustomizationSession.h",
        root / "Editor" / "include" / "SagaEditor" / "Customization" / "WorkspaceCustomizationController.h",
        root / "Editor" / "include" / "SagaEditor" / "Customization" / "WorkspaceCustomizationModel.h",
        root / "Editor" / "include" / "SagaEditor" / "Customization" / "WorkspaceCustomizationOverlayPolicy.h",
        root / "Editor" / "include" / "SagaEditor" / "Customization" / "WorkspaceCustomizationOverlayStore.h",
        root / "Editor" / "include" / "SagaEditor" / "Customization" / "WorkspaceCustomizationFeedback.h",
        root / "Editor" / "include" / "SagaEditor" / "Customization" / "WorkspaceCustomizationPanelViewModel.h",
        root / "Editor" / "include" / "SagaEditor" / "Customization" / "WorkspaceCustomizationSession.h",
        root / "Editor" / "src" / "SagaEditor" / "Customization" / "EditorCustomizationCapability.cpp",
        root / "Editor" / "src" / "SagaEditor" / "Customization" / "EditorCustomizationDiagnostics.cpp",
        root / "Editor" / "src" / "SagaEditor" / "Customization" / "EditorCustomizationOverlayLoader.cpp",
        root / "Editor" / "src" / "SagaEditor" / "Customization" / "ShortcutCustomizationController.cpp",
        root / "Editor" / "src" / "SagaEditor" / "Customization" / "ShortcutCustomizationFeedback.cpp",
        root / "Editor" / "src" / "SagaEditor" / "Customization" / "ShortcutCustomizationModel.cpp",
        root / "Editor" / "src" / "SagaEditor" / "Customization" / "ShortcutCustomizationPanelViewModel.cpp",
        root / "Editor" / "src" / "SagaEditor" / "Customization" / "ShortcutCustomizationSession.cpp",
        root / "Editor" / "src" / "SagaEditor" / "Customization" / "WorkspaceCustomizationController.cpp",
        root / "Editor" / "src" / "SagaEditor" / "Customization" / "WorkspaceCustomizationModel.cpp",
        root / "Editor" / "src" / "SagaEditor" / "Customization" / "WorkspaceCustomizationOverlayPolicy.cpp",
        root / "Editor" / "src" / "SagaEditor" / "Customization" / "WorkspaceCustomizationOverlayStore.cpp",
        root / "Editor" / "src" / "SagaEditor" / "Customization" / "WorkspaceCustomizationFeedback.cpp",
        root / "Editor" / "src" / "SagaEditor" / "Customization" / "WorkspaceCustomizationPanelViewModel.cpp",
        root / "Editor" / "src" / "SagaEditor" / "Customization" / "WorkspaceCustomizationSession.cpp",
    };
    const std::vector<std::string> forbiddenTokens = {
        "QApplication",
        "QWidget",
        "Qt6::",
        "#include <Q",
        "SystemDefinitionEngine",
        "SDE/",
        "Forge/",
        "SagaPipeline/",
    };
    std::vector<std::string> offenders;
    for (const auto& file : files)
    {
        const std::string text = ReadText(file);
        for (const std::string& token : forbiddenTokens)
        {
            if (text.find(token) != std::string::npos)
            {
                offenders.push_back(file.generic_string() + ": " + token);
                break;
            }
        }
    }

    EXPECT_TRUE(offenders.empty())
        << "Safe customization core must stay Qt-free and tool-free. "
        << "First offender: "
        << (offenders.empty() ? "" : offenders.front());
}

TEST(CMakeTargetBoundaryTests, CustomizeWorkspacePanelDoesNotInvokeToolchain)
{
    const auto root = std::filesystem::path(SAGA_SOURCE_ROOT);
    const auto offenders = FindForbiddenText(
        root / "Editor" / "src" / "SagaEditor" / "UI" / "Qt" / "Panels",
        {
            "SystemDefinitionEngine",
            "SDE/",
            "Forge/",
            "SagaPipeline/",
            "saga-editor-composition-compiler",
            "saga-pipeline",
        });

    EXPECT_TRUE(offenders.empty())
        << "Qt editor panels must not invoke or include composition toolchain internals. "
        << "First offender: "
        << (offenders.empty() ? "" : offenders.front());
}

TEST(CMakeTargetBoundaryTests, AppsEditorDoesNotInvokeCompositionToolchain)
{
    const auto root = std::filesystem::path(SAGA_SOURCE_ROOT);
    const auto offenders = FindForbiddenText(
        root / "Apps" / "Editor",
        {
            "saga-pipeline",
            "saga-editor-composition-compiler",
            "SystemDefinitionEngine",
            "Forge/",
            "SagaPipeline/",
        });

    EXPECT_TRUE(offenders.empty())
        << "Apps/Editor may consume composition manifests but must not invoke the toolchain. "
        << "First offender: "
        << (offenders.empty() ? "" : offenders.front());
}

TEST(CMakeTargetBoundaryTests, SagaEditorComposerIsAnAppAndUsesToolBoundaries)
{
    const auto root = std::filesystem::path(SAGA_SOURCE_ROOT);
    const auto appRoot = root / "Apps" / "SagaEditorComposer";
    ASSERT_TRUE(std::filesystem::exists(appRoot))
        << "SagaEditorComposer must be a product-level app under Apps/.";
    EXPECT_FALSE(std::filesystem::exists(root / "Tools" / "SagaEditorComposer"))
        << "The visual Composer must not be placed under Tools/.";

    std::vector<std::string> offenders;
    for (const auto& directory : {
             appRoot,
             root / "Editor" / "include" / "SagaEditor" / "Composer",
             root / "Editor" / "src" / "SagaEditor" / "Composer",
         })
    {
        const auto hits = FindForbiddenText(
            directory,
            {
                "#include \"SDE/",
                "#include <SDE/",
                "Tools/SystemDefinitionEngine",
                "#include \"Forge/",
                "#include <Forge/",
                "#include \"SagaPipeline/",
                "#include <SagaPipeline/",
                "#include \"SagaEditorComposition/",
                "#include <SagaEditorComposition/",
            });
        offenders.insert(offenders.end(), hits.begin(), hits.end());
    }

    EXPECT_TRUE(offenders.empty())
        << "SagaEditorComposer must invoke tools through process boundaries and "
           "must not include SDE, Forge, SagaPipeline, or SagaEditorComposition "
           "internals. First offender: "
        << (offenders.empty() ? "" : offenders.front());
}

TEST(CMakeTargetBoundaryTests, SagaEditorComposerDoesNotWriteGeneratedArtifacts)
{
    const auto root = std::filesystem::path(SAGA_SOURCE_ROOT);
    const auto offenders = FindForbiddenText(
        root / "Editor" / "src" / "SagaEditor" / "Composer",
        {
            "std::ofstream output(paths.artifactPath",
            "std::ofstream output(paths.manifestPath",
            "std::ofstream output(paths.diagnosticsPath",
            "std::ofstream output(paths.sourceMapPath",
            "std::ofstream output(paths.dependenciesPath",
        });

    EXPECT_TRUE(offenders.empty())
        << "Composer core may read generated artifact summaries, but source "
           "editing must not target generated artifact JSON. First offender: "
        << (offenders.empty() ? "" : offenders.front());
}

TEST(CMakeTargetBoundaryTests, SagaPipelineStaysHeadlessAndRuntimeFree)
{
    const auto root = std::filesystem::path(SAGA_SOURCE_ROOT);
    const auto offenders = FindForbiddenText(
        root / "Tools" / "SagaPipeline",
        {
            "QApplication",
            "Qt",
            "SagaEditor/",
            "SagaEngine/Runtime",
            "SagaServer/",
        });

    EXPECT_TRUE(offenders.empty())
        << "SagaPipeline must stay headless and must not depend on editor/runtime internals. "
        << "First offender: "
        << (offenders.empty() ? "" : offenders.front());
}

TEST(CMakeTargetBoundaryTests, SagaEditorCompositionToolStaysSagaOwnedAndHeadless)
{
    const auto root = std::filesystem::path(SAGA_SOURCE_ROOT);
    const auto toolRoot = root / "Tools" / "SagaEditorComposition";
    ASSERT_TRUE(std::filesystem::exists(toolRoot))
        << "SagaEditorComposition must live outside the SDE package tree.";

    EXPECT_FALSE(std::filesystem::exists(
        root / "Tools" / "SystemDefinitionEngine" / "include" /
        "SagaEditorComposition"))
        << "Saga-specific editor composition headers must not live under SDE.";
    EXPECT_FALSE(std::filesystem::exists(
        root / "Tools" / "SystemDefinitionEngine" / "schema_packages" /
        "saga.editor"))
        << "saga.editor schemas must not live under SDE.";

    const auto offenders = FindForbiddenText(
        toolRoot,
        {
            "QApplication",
            "QWidget",
            "Qt6::",
            "#include <Q",
            "#include \"SagaEditor/",
            "#include <SagaEditor/",
            "Forge/",
            "Prism/",
            "SagaPipeline/",
            "Apps/Editor",
        });

    EXPECT_TRUE(offenders.empty())
        << "SagaEditorComposition must stay a headless Saga-owned adapter and "
           "must not depend on Qt, editor runtime, Forge, Prism, or SagaPipeline "
           "internals. First offender: "
        << (offenders.empty() ? "" : offenders.front());
}
