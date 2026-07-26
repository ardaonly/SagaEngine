// SPDX-License-Identifier: MPL-2.0
#include <gtest/gtest.h>

#include <array>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{
std::string ReadText(const std::filesystem::path& path)
{
    std::ifstream input(path);
    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

std::string ExtractTargetCalls(
    std::string_view text,
    std::string_view command,
    std::string_view target)
{
    const std::string marker =
        std::string(command) + "(" + std::string(target);
    std::string calls;
    auto cursor = text.find(marker);
    while (cursor != std::string_view::npos)
    {
        const auto afterTarget = cursor + marker.size();
        if (afterTarget < text.size() &&
            !std::isspace(static_cast<unsigned char>(text[afterTarget])) &&
            text[afterTarget] != ')')
        {
            cursor = text.find(marker, afterTarget);
            continue;
        }

        std::size_t depth = 0;
        auto end = cursor;
        for (; end < text.size(); ++end)
        {
            if (text[end] == '(')
            {
                ++depth;
            }
            else if (text[end] == ')' && --depth == 0)
            {
                ++end;
                break;
            }
        }
        calls.append(text.substr(cursor, end - cursor));
        calls.push_back('\n');
        cursor = text.find(marker, end);
    }
    return calls;
}

bool ContainsCMakeToken(std::string_view text, std::string_view token)
{
    const auto isTokenCharacter = [](char value) {
        return std::isalnum(static_cast<unsigned char>(value)) != 0 ||
               value == '_' || value == '-' || value == ':';
    };
    auto cursor = text.find(token);
    while (cursor != std::string_view::npos)
    {
        const bool leftBoundary =
            cursor == 0 || !isTokenCharacter(text[cursor - 1]);
        const auto right = cursor + token.size();
        const bool rightBoundary =
            right == text.size() || !isTokenCharacter(text[right]);
        if (leftBoundary && rightBoundary)
        {
            return true;
        }
        cursor = text.find(token, right);
    }
    return false;
}
}

TEST(CMakeTargetBoundaryTests, EverySourceModuleOwnsAnExplicitManifest)
{
    const auto root = std::filesystem::path(SAGA_SOURCE_ROOT);
    const std::array moduleGroups = {
        root / "Engine/Source/Runtime",
        root / "Engine/Source/Editor",
        root / "Engine/Source/Programs",
    };

    std::vector<std::filesystem::path> missingManifests;
    std::vector<std::filesystem::path> implicitManifests;
    for (const auto& group : moduleGroups)
    {
        ASSERT_TRUE(std::filesystem::is_directory(group)) << group;
        for (const auto& entry : std::filesystem::directory_iterator(group))
        {
            if (!entry.is_directory())
            {
                continue;
            }
            const auto manifest = entry.path() / "CMakeLists.txt";
            if (!std::filesystem::is_regular_file(manifest))
            {
                missingManifests.push_back(manifest);
                continue;
            }
            const auto text = ReadText(manifest);
            if (text.find("SOURCES") == std::string::npos ||
                text.find("GLOB") != std::string::npos)
            {
                implicitManifests.push_back(manifest);
            }
        }
    }

    EXPECT_TRUE(missingManifests.empty()) << "Every module needs CMakeLists.txt";
    EXPECT_TRUE(implicitManifests.empty()) << "Module manifests must list owned sources explicitly";
}

TEST(CMakeTargetBoundaryTests, ModuleRegistrationKeepsAggregateTargets)
{
    const auto root = std::filesystem::path(SAGA_SOURCE_ROOT);
    const auto moduleSupport = ReadText(root / "cmake/modules/SagaModule.cmake");
    for (const auto* functionName : {
             "saga_add_runtime_module",
             "saga_add_editor_module",
             "saga_add_program",
             "saga_apply_registered_module"})
    {
        EXPECT_NE(moduleSupport.find(functionName), std::string::npos) << functionName;
    }

    const auto targets = ReadText(root / "cmake/modules/SagaTargets.cmake");
    for (const auto* target : {
             "SagaEngine", "SagaRuntimeLib", "SagaServerLib", "SagaEditorLib"})
    {
        EXPECT_NE(targets.find(target), std::string::npos) << target;
    }
}

TEST(CMakeTargetBoundaryTests, RuntimeAggregatesComposeOnlyOwnedModuleObjects)
{
    const auto root = std::filesystem::path(SAGA_SOURCE_ROOT);
    const auto moduleSupport = ReadText(root / "cmake/modules/SagaModule.cmake");
    const auto targets = ReadText(root / "cmake/modules/SagaTargets.cmake");

    EXPECT_NE(
        moduleSupport.find("source must stay below"),
        std::string::npos);
    EXPECT_NE(
        moduleSupport.find("$<TARGET_OBJECTS:${_object_target}>"),
        std::string::npos);

    for (const auto* target : {
             "SagaEngine", "SagaRuntimeLib", "SagaServerLib"})
    {
        const auto composition =
            "saga_compose_registered_objects(" + std::string(target) + ")";
        EXPECT_NE(targets.find(composition), std::string::npos) << target;
    }

    for (const auto* legacySourceList : {
             "${ENGINE_SOURCES}",
             "${RUNTIME_SOURCES}",
             "${SERVER_SOURCES}"})
    {
        EXPECT_EQ(targets.find(legacySourceList), std::string::npos)
            << legacySourceList;
    }
}

TEST(CMakeTargetBoundaryTests, RuntimeVendorDependenciesMatchHeaderVisibility)
{
    const auto root = std::filesystem::path(SAGA_SOURCE_ROOT);
    const auto math =
        ReadText(root / "Engine/Source/Runtime/Math/CMakeLists.txt");
    EXPECT_NE(math.find("PUBLIC_DEPS glm::glm"), std::string::npos);

    for (const auto& [module, dependency] :
         std::array<std::pair<std::string_view, std::string_view>, 5>{
             std::pair{"Assets", "nlohmann_json::nlohmann_json"},
             std::pair{"Resources", "nlohmann_json::nlohmann_json"},
             std::pair{"Scripting", "nlohmann_json::nlohmann_json"},
             std::pair{"Networking", "asio::asio"},
             std::pair{"ServerAuthority", "asio::asio"}})
    {
        const auto manifest = ReadText(
            root / "Engine/Source/Runtime" / module / "CMakeLists.txt");
        const auto privateDeps = manifest.find("PRIVATE_DEPS");
        ASSERT_NE(privateDeps, std::string::npos) << module;
        EXPECT_NE(manifest.find(dependency, privateDeps), std::string::npos)
            << module << " needs " << dependency;
    }

    const auto targets = ReadText(root / "cmake/modules/SagaTargets.cmake");
    const auto backendLinks =
        ExtractTargetCalls(targets, "target_link_libraries", "SagaBackend");
    EXPECT_TRUE(ContainsCMakeToken(backendLinks, "libpqxx::pqxx"));
    EXPECT_TRUE(
        ContainsCMakeToken(backendLinks, "redis++::redis++_static"));
    EXPECT_FALSE(ContainsCMakeToken(backendLinks, "SDL2::SDL2"));
    EXPECT_FALSE(ContainsCMakeToken(backendLinks, "imgui::imgui"));
}

TEST(CMakeTargetBoundaryTests, EditorAggregateComposesOnlyModuleObjects)
{
    const auto root = std::filesystem::path(SAGA_SOURCE_ROOT);
    const auto targets = ReadText(root / "cmake/modules/SagaTargets.cmake");

    EXPECT_NE(
        targets.find("saga_compose_registered_objects(SagaEditorLib)"),
        std::string::npos);
    EXPECT_EQ(targets.find("${EDITOR_SOURCES}"), std::string::npos);

    const auto editorQt =
        ReadText(root / "Engine/Source/Editor/EditorQt/CMakeLists.txt");
    const auto publicDeps = editorQt.find("PUBLIC_DEPS");
    ASSERT_NE(publicDeps, std::string::npos);
    EXPECT_NE(editorQt.find("Qt6::Core", publicDeps), std::string::npos);
    EXPECT_NE(editorQt.find("Qt6::Widgets", publicDeps), std::string::npos);

    for (const auto& module : {
             "EditorCore", "EditorFramework", "EditorAuthoring"})
    {
        const auto manifest = ReadText(
            root / "Engine/Source/Editor" / module / "CMakeLists.txt");
        EXPECT_NE(manifest.find("PRIVATE_DEPS"), std::string::npos)
            << module;
    }
}

TEST(CMakeTargetBoundaryTests, RetiredPropertyEditorBackendSeamStaysAbsent)
{
    const auto root = std::filesystem::path(SAGA_SOURCE_ROOT);
    const auto inspectorRoot =
        root / "Engine/Source/Editor/EditorAuthoring";
    const auto manifest = ReadText(inspectorRoot / "CMakeLists.txt");

    for (const auto* retired : {
             "IPropertyEditor.h",
             "IPropertyEditorBackend.h",
             "PropertyEditorFactory.h",
             "PropertyEditorFactory.cpp"})
    {
        EXPECT_FALSE(std::filesystem::exists(
            inspectorRoot / "Public/SagaEditor/InspectorEditing" / retired));
        EXPECT_FALSE(std::filesystem::exists(
            inspectorRoot / "Private/SagaEditor/InspectorEditing" / retired));
        EXPECT_EQ(manifest.find(retired), std::string::npos) << retired;
    }

    const auto inspectorTests =
        ReadText(inspectorRoot / "Tests/InspectorEditingTests.cpp");
    for (const auto* retiredSymbol : {
             "IPropertyEditor",
             "IPropertyEditorBackend",
             "PropertyEditorFactory",
             "SetBackend(",
             "g_backend"})
    {
        EXPECT_EQ(inspectorTests.find(retiredSymbol), std::string::npos)
            << retiredSymbol;
        EXPECT_EQ(manifest.find(retiredSymbol), std::string::npos)
            << retiredSymbol;
    }
}

TEST(CMakeTargetBoundaryTests, MainWindowContractOwnsChromeNotProjectDialogs)
{
    const auto root = std::filesystem::path(SAGA_SOURCE_ROOT);
    const auto interfaceText = ReadText(root /
        "Engine/Source/Editor/EditorFramework/Public/SagaEditor/UI/IUIMainWindow.h");
    const auto qtHeader = ReadText(root /
        "Engine/Source/Editor/EditorQt/Public/SagaEditorQt/QtUIMainWindow.h");
    const auto qtSource = ReadText(root /
        "Engine/Source/Editor/EditorQt/Private/SagaEditorQt/QtUIMainWindow.cpp");

    for (const auto* retiredDialogMember : {
             "NewProjectPrompt",
             "UnsavedDocumentsDecision",
             "ChooseProjectToOpen",
             "ChooseNewProject",
             "ConfirmUnsavedDocuments"})
    {
        EXPECT_EQ(interfaceText.find(retiredDialogMember), std::string::npos)
            << retiredDialogMember;
        EXPECT_EQ(qtHeader.find(retiredDialogMember), std::string::npos)
            << retiredDialogMember;
        EXPECT_EQ(qtSource.find(retiredDialogMember), std::string::npos)
            << retiredDialogMember;
    }
    for (const auto* retainedWindowRole : {
             "ApplyShellLayout",
             "SetStatusMessage",
             "DockPanel",
             "SaveState",
             "SetOnClose"})
    {
        EXPECT_NE(interfaceText.find(retainedWindowRole), std::string::npos)
            << retainedWindowRole;
    }
}

TEST(CMakeTargetBoundaryTests, QtCompositionRootStaysConcrete)
{
    const auto root = std::filesystem::path(SAGA_SOURCE_ROOT);
    const auto frameworkRoot =
        root / "Engine/Source/Editor/EditorFramework";
    const auto qtRoot = root / "Engine/Source/Editor/EditorQt";
    const auto frameworkManifest =
        ReadText(frameworkRoot / "CMakeLists.txt");
    const auto qtManifest = ReadText(qtRoot / "CMakeLists.txt");
    const auto appHeader = ReadText(frameworkRoot /
        "Public/SagaEditor/Host/EditorApp.h");
    const auto shellHeader = ReadText(frameworkRoot /
        "Public/SagaEditor/Shell/EditorShell.h");
    const auto runner = ReadText(qtRoot /
        "Private/SagaEditorQt/Host/EditorApplicationRunner.cpp");

    for (const auto* retired : {
             "IUIFactory", "QtUIFactory",
             "IUIApplication", "QtUIApplication"})
    {
        EXPECT_EQ(frameworkManifest.find(retired), std::string::npos)
            << retired;
        EXPECT_EQ(qtManifest.find(retired), std::string::npos) << retired;
        EXPECT_EQ(appHeader.find(retired), std::string::npos) << retired;
        EXPECT_EQ(shellHeader.find(retired), std::string::npos) << retired;
    }
    EXPECT_EQ(appHeader.find("RunSmoke"), std::string::npos);
    EXPECT_EQ(appHeader.find("int Run("), std::string::npos);
    for (const auto* concreteOwner : {
             "QApplication", "QTimer::singleShot",
             "QtSettingsStore", "QtUIMainWindow"})
    {
        EXPECT_NE(runner.find(concreteOwner), std::string::npos)
            << concreteOwner;
    }
    EXPECT_NE(qtManifest.find("EditorApplicationRunner.cpp"),
              std::string::npos);
    EXPECT_EQ(frameworkManifest.find("EditorApplicationRunner.cpp"),
              std::string::npos);
}

TEST(CMakeTargetBoundaryTests, ProductAggregateComposesOnlyProgramObjects)
{
    const auto root = std::filesystem::path(SAGA_SOURCE_ROOT);
    const auto targets = ReadText(root / "cmake/modules/SagaTargets.cmake");

    EXPECT_NE(
        targets.find("saga_compose_registered_objects(SagaProductLib)"),
        std::string::npos);
    EXPECT_EQ(targets.find("${SAGA_PRODUCT_SOURCES}"), std::string::npos);

    for (const auto& manifest : {
             root / "Engine/Source/Programs/SagaLauncher/CMakeLists.txt",
             root / "Engine/Source/Editor/EditorCollaboration/CMakeLists.txt"})
    {
        EXPECT_NE(ReadText(manifest).find("PRIVATE_DEPS"), std::string::npos)
            << manifest;
    }

    const auto install = ReadText(root / "cmake/modules/SagaInstall.cmake");
    EXPECT_EQ(install.find("Module"), std::string::npos)
        << "Module object targets must not be installed or exported";
}

TEST(CMakeTargetBoundaryTests, ServerTargetAvoidsClientAndVendorBackends)
{
    const auto root = std::filesystem::path(SAGA_SOURCE_ROOT);
    const auto targets = ReadText(root / "cmake/modules/SagaTargets.cmake");
    const auto serverLinks =
        ExtractTargetCalls(targets, "target_link_libraries", "SagaServerLib");
    ASSERT_FALSE(serverLinks.empty());

    for (const auto* forbidden : {"SagaPlatformSDL", "SagaBackend"})
    {
        EXPECT_FALSE(ContainsCMakeToken(serverLinks, forbidden))
            << "SagaServerLib directly links forbidden client/vendor target "
            << forbidden;
    }
    for (const auto* forbiddenFamily : {
             "Qt6::", "qt::", "SDL2::", "RmlUi::", "rmlui::",
             "Diligent"})
    {
        EXPECT_EQ(serverLinks.find(forbiddenFamily), std::string::npos)
            << "SagaServerLib directly links forbidden target family "
            << forbiddenFamily;
    }
}

TEST(CMakeTargetBoundaryTests, RuntimeAndHeadlessTargetsExcludeAuthoringToolchains)
{
    const auto root = std::filesystem::path(SAGA_SOURCE_ROOT);
    const auto targets = ReadText(root / "cmake/modules/SagaTargets.cmake");

    std::string targetContracts;
    for (const auto& [command, target] :
         std::array<std::pair<std::string_view, std::string_view>, 8>{
             std::pair{"add_executable", "SagaRuntime"},
             std::pair{"target_include_directories", "SagaRuntime"},
             std::pair{"target_link_libraries", "SagaRuntime"},
             std::pair{"add_library", "MultiplayerSandboxHeadlessLib"},
             std::pair{"target_sources", "MultiplayerSandboxHeadlessLib"},
             std::pair{"target_include_directories", "MultiplayerSandboxHeadlessLib"},
             std::pair{"target_link_libraries", "MultiplayerSandboxHeadlessLib"},
             std::pair{"target_link_libraries", "MultiplayerSandboxHeadless"}})
    {
        targetContracts += ExtractTargetCalls(targets, command, target);
    }

    for (const auto* forbidden : {
             "Qt6::", "qt::", "SagaEditor", "SagaProductLib",
             "SagaProjectKit", "SagaPackager", "SagaScript",
             "VisualBlocks", "ProjectManifestLoader", "PackageResolver",
             "SourceCompiler", "BlockDescriptor", "ProjectLoader",
             "PackageResolutionService", "ContentCompiler", "ProjectWorker",
             "BlockProjection"})
    {
        EXPECT_EQ(targetContracts.find(forbidden), std::string::npos)
            << "Runtime and headless targets must not include or link "
            << forbidden;
    }

    EXPECT_EQ(targetContracts.find("Samples/StarterArena"), std::string::npos);
    const auto runtimeProgram = ReadText(
        root / "Engine/Source/Programs/SagaRuntime/Private/RuntimeApplication.cpp");
    EXPECT_EQ(runtimeProgram.find("StarterArena"), std::string::npos);
    EXPECT_EQ(runtimeProgram.find(".sagaproj"), std::string::npos);
}

TEST(CMakeTargetBoundaryTests, GraphicsAggregateKeepsNativeLifecyclePrivate)
{
    const auto root = std::filesystem::path(SAGA_SOURCE_ROOT);
    const auto targets = ReadText(root / "cmake/modules/SagaTargets.cmake");
    const auto graphicsTargets =
        ReadText(root / "cmake/modules/SagaGraphicsTargets.cmake");
    const auto rhiManifest =
        ReadText(root / "Engine/Source/Runtime/RHI/CMakeLists.txt");
    const auto renderManifest =
        ReadText(root / "Engine/Source/Runtime/Render/CMakeLists.txt");

    EXPECT_NE(
        graphicsTargets.find("add_library(SagaGraphics INTERFACE)"),
        std::string::npos);
    const auto aggregateLinks = ExtractTargetCalls(
        graphicsTargets, "target_link_libraries", "SagaGraphics");
    EXPECT_TRUE(ContainsCMakeToken(aggregateLinks, "SagaGraphicsCore"));
    EXPECT_FALSE(ContainsCMakeToken(aggregateLinks, "SagaDiligentRuntime"));

    EXPECT_NE(
        targets.find("add_library(SagaDiligentRuntime STATIC)"),
        std::string::npos);
    EXPECT_NE(
        targets.find(
            "saga_get_registered_sources(SagaDiligentRuntime"),
        std::string::npos);
    EXPECT_NE(
        targets.find("target_sources(SagaDiligentRuntime PRIVATE"),
        std::string::npos);
    EXPECT_NE(
        rhiManifest.find("TARGET SagaDiligentRuntime"),
        std::string::npos);
    EXPECT_NE(
        renderManifest.find("TARGET SagaDiligentRuntime"),
        std::string::npos);

    const auto privateLinks = ExtractTargetCalls(
        graphicsTargets, "target_link_libraries", "SagaGraphicsPrivate");
    EXPECT_TRUE(ContainsCMakeToken(privateLinks, "SagaDiligentRuntime"));
}

TEST(CMakeTargetBoundaryTests, DiligentPrivateTestsHaveDedicatedTargets)
{
    const auto root = std::filesystem::path(SAGA_SOURCE_ROOT);
    const auto tests =
        ReadText(root / "cmake/modules/SagaTests.cmake") +
        ReadText(root / "cmake/modules/tests/SagaTestSources.cmake") +
        ReadText(root / "cmake/modules/tests/SagaCoreTestTargets.cmake") +
        ReadText(root / "cmake/modules/tests/SagaRuntimeTestTargets.cmake") +
        ReadText(root / "cmake/modules/tests/SagaProductEditorTestTargets.cmake") +
        ReadText(root / "cmake/modules/tests/SagaEvidenceTestTargets.cmake");

    EXPECT_NE(
        tests.find("add_executable(SagaDiligentWhiteboxTests"),
        std::string::npos);
    EXPECT_NE(
        tests.find("add_executable(SagaDiligentGpuIntegrationTests"),
        std::string::npos);
    EXPECT_NE(tests.find("whitebox-private"), std::string::npos);
    EXPECT_NE(tests.find("SAGA_ENABLE_GPU_TEST_EXECUTION"), std::string::npos);
    EXPECT_NE(
        tests.find("Engine/Source/Runtime/RHI/Tests/GraphicsDiligentBackend"),
        std::string::npos);
    EXPECT_NE(
        tests.find("Engine/Source/Runtime/Render/Tests/DiligentFrameSlotTrackerTests.cpp"),
        std::string::npos);
    EXPECT_EQ(
        tests.find("Tests/Unit/Render/GraphicsDiligentBackend"),
        std::string::npos);

    const auto unitLinks =
        ExtractTargetCalls(tests, "target_link_libraries", "SagaUnitTests");
    EXPECT_FALSE(ContainsCMakeToken(unitLinks, "SagaDiligentBackend"));
    EXPECT_FALSE(ContainsCMakeToken(unitLinks, "SagaDiligentRuntime"));
    EXPECT_FALSE(ContainsCMakeToken(unitLinks, "SagaGraphicsPrivate"));

    const auto integrationLinks = ExtractTargetCalls(
        tests, "target_link_libraries", "SagaIntegrationTests");
    EXPECT_FALSE(ContainsCMakeToken(integrationLinks, "SagaDiligentBackend"));
    EXPECT_FALSE(ContainsCMakeToken(integrationLinks, "SagaDiligentRuntime"));
    EXPECT_FALSE(ContainsCMakeToken(integrationLinks, "SagaGraphicsPrivate"));
    EXPECT_FALSE(ContainsCMakeToken(integrationLinks, "SDL2::SDL2"));
}

TEST(CMakeTargetBoundaryTests, CSharpHostOwnersStayCompleteAcrossFocusedTargets)
{
    const auto root = std::filesystem::path(SAGA_SOURCE_ROOT);
    const auto module = ReadText(root / "Engine/Source/Runtime/Scripting/CMakeLists.txt");
    const auto tests = ReadText(root / "cmake/modules/tests/SagaRuntimeTestTargets.cmake");

    for (const auto* source : {
             "CSharpScriptHost.cpp",
             "CSharpScriptHostCallbacks.cpp",
             "CSharpScriptHostCoreClr.cpp",
             "CSharpScriptHostInstances.cpp",
             "CSharpScriptHostInvocation.cpp",
             "CSharpScriptHostSupport.cpp",
             "CSharpScriptHostVerifiedGeneration.cpp",
         })
    {
        EXPECT_NE(module.find(source), std::string::npos) << source;
        const auto first = tests.find(source);
        ASSERT_NE(first, std::string::npos) << source;
        EXPECT_NE(tests.find(source, first + 1), std::string::npos) << source;
    }
}

TEST(CMakeTargetBoundaryTests, LegacyOwnershipRootsStayAbsent)
{
    const auto root = std::filesystem::path(SAGA_SOURCE_ROOT);
    for (const auto& legacy : std::initializer_list<std::string>{
             "Runtime", "Server", "Backends", "Collaboration", "Shared",
             "Editor", "Apps", "Content", "core", "docs", "samples",
             std::string("Tools") + "/scripts"})
    {
        EXPECT_FALSE(std::filesystem::exists(root / legacy)) << legacy;
    }
}

TEST(CMakeTargetBoundaryTests, OwnershipMetadataNamesCurrentRepositoryOwners)
{
    const auto root = std::filesystem::path(SAGA_SOURCE_ROOT);
    const auto targets = ReadText(root / "cmake/modules/SagaTargets.cmake");
    EXPECT_EQ(targets.find("FOLDER      \"Apps\""), std::string::npos);
    for (const auto* folder : {
             "Programs/SagaLauncher",
             "Programs/SagaRuntime",
             "Programs/SagaEditor",
             "Programs/SagaEditorLab",
             "Programs/SagaSandbox",
             "Developer/SagaDev"})
    {
        EXPECT_NE(targets.find(folder), std::string::npos) << folder;
    }

    const auto clangTidy = ReadText(root / ".clang-tidy");
    EXPECT_NE(
        clangTidy.find("Engine/Source|Samples|Tests|Tools"),
        std::string::npos);
    EXPECT_EQ(
        clangTidy.find("Engine|Server|Apps|Backends|Editor|Tests"),
        std::string::npos);
}

TEST(CMakeTargetBoundaryTests, EmptyOwnershipPlaceholdersStayAbsent)
{
    const auto root = std::filesystem::path(SAGA_SOURCE_ROOT);
    for (const auto& placeholder : {
             root / "Engine/Source/Editor/EditorScripting",
             root / "Engine/Source/Programs/SagaServer",
             root / "Tests/Unit/Editor/UnsupportedSyntaxTests.cpp",
             root / "Tests/Unit/Editor/NodeValidationTests.cpp",
             root / "Tests/Unit/Editor/RoundTripTests.cpp",
             root / "Tests/Unit/Editor/GraphDebuggerTests.cpp",
             root / "Tests/Unit/Editor/GraphCompilationTests.cpp",
             root / "Tests/Unit/Editor/PinConnectionTests.cpp",
             root / "Tests/Unit/Runtime/RenderClientAppTests.cpp",
             root / "Tools/SagaScript/Public",
             root / "Tools/SagaScript/Private",
             root / "Tools/SagaPackager/Public",
             root / "Tools/SagaPackager/Private",
             root / "Engine/Source/Editor/EditorCore/Private/SagaEditor/Project/ProjectIndexer.cpp",
             root / "Engine/Source/Editor/EditorAuthoring/Private/SagaEditor/Importers/AssetImportManager.cpp",
             root / "Engine/Source/Editor/EditorAuthoring/Private/SagaEditor/Pipeline/AssetCooking.cpp",
             root / "Engine/Source/Editor/EditorAuthoring/Private/SagaEditor/Pipeline/ContentPipeline.cpp",
             root / "Engine/Source/Editor/EditorAuthoring/Private/SagaEditor/Pipeline/DerivedDataCache.cpp",
             root / "Engine/Source/Editor/EditorAuthoring/Private/SagaEditor/PrefabEditing/PrefabEditor.cpp",
             root / "Engine/Source/Editor/EditorAuthoring/Private/SagaEditor/PrefabEditing/PrefabInstanceTracker.cpp",
             root / "Engine/Source/Editor/EditorAuthoring/Private/SagaEditor/WorldEditing/WorldEditor.cpp",
             root / "Engine/Source/Editor/EditorAuthoring/Private/SagaEditor/WorldEditing/WorldStreamingEditor.cpp"})
    {
        EXPECT_FALSE(std::filesystem::exists(placeholder)) << placeholder;
    }
}

TEST(CMakeTargetBoundaryTests, PlayIsVerifyOnlyLifetimeSafeAndRuntimeUsesWorkerManifests)
{
    const auto root = std::filesystem::path(SAGA_SOURCE_ROOT);
    const auto coordinator = ReadText(root /
        "Engine/Source/Editor/EditorAuthoring/Private/SagaEditor/Scripting/ScriptAnalysisCoordinator.cpp");
    const auto controller = ReadText(root /
        "Engine/Source/Editor/EditorAuthoring/Private/SagaEditor/Play/RuntimeProcessController.cpp");
    const auto runtimeHost = ReadText(root /
        "Engine/Source/Programs/SagaRuntime/Private/RuntimeHost.cpp");
    const auto frameLoop = ReadText(root /
        "Engine/Source/Programs/SagaRuntime/Private/RuntimeFrameLoop.cpp");
    EXPECT_NE(coordinator.find("\"lock-verify\""), std::string::npos);
    EXPECT_EQ(coordinator.find("{\"resolve\""), std::string::npos);
    EXPECT_EQ(controller.find("&workspace"), std::string::npos);
    EXPECT_NE(controller.find("Play.Runtime.ResultMissing"), std::string::npos);
    EXPECT_NE(controller.find("ParseRuntimeResultV1"), std::string::npos);
    EXPECT_NE(controller.find("ParseRuntimeStartupReadyV1"), std::string::npos);
    EXPECT_NE(controller.find("RuntimePlayState::Starting"), std::string::npos);
    EXPECT_NE(controller.find("userStopRequested"), std::string::npos);
    EXPECT_NE(controller.find("Play.Runtime.RuntimeTimeout"), std::string::npos);
    EXPECT_NE(controller.find("Play.Runtime.ReportedFailure"), std::string::npos);
    EXPECT_NE(controller.find("removeStale(prepared.resultPath)"), std::string::npos);
    EXPECT_EQ(runtimeHost.find("script_artifacts.json"), std::string::npos);
    EXPECT_EQ(runtimeHost.find("script_capabilities.json"), std::string::npos);
    EXPECT_NE(frameLoop.find("LoadVerifiedGeneration"), std::string::npos);
    EXPECT_NE(frameLoop.find("startupReadyPath"), std::string::npos);
    EXPECT_NE(frameLoop.find("publishReady"), std::string::npos);
}

TEST(CMakeTargetBoundaryTests, RuntimeHostComposesCohesivePrivateSessions)
{
    const auto root = std::filesystem::path(SAGA_SOURCE_ROOT);
    const auto manifest = ReadText(root /
        "Engine/Source/Programs/SagaRuntime/CMakeLists.txt");
    const auto host = ReadText(root /
        "Engine/Source/Programs/SagaRuntime/Private/RuntimeHost.cpp");
    const auto launch = ReadText(root /
        "Engine/Source/Programs/SagaRuntime/Private/RuntimeLaunchSession.cpp");
    const auto artifacts = ReadText(root /
        "Engine/Source/Programs/SagaRuntime/Private/RuntimeArtifactSession.cpp");
    const auto render = ReadText(root /
        "Engine/Source/Programs/SagaRuntime/Private/RuntimeRenderSession.cpp");
    const auto script = ReadText(root /
        "Engine/Source/Programs/SagaRuntime/Private/RuntimeScriptSession.cpp");
    const auto ui = ReadText(root /
        "Engine/Source/Programs/SagaRuntime/Private/RuntimeUiSession.cpp");
    const auto evidence = ReadText(root /
        "Engine/Source/Programs/SagaRuntime/Private/RuntimeEvidenceSession.cpp");
    const auto frameLoop = ReadText(root /
        "Engine/Source/Programs/SagaRuntime/Private/RuntimeFrameLoop.cpp");

    for (const auto* source : {
             "RuntimeLaunchSession.cpp", "RuntimeArtifactSession.cpp",
             "RuntimeRenderSession.cpp", "RuntimeScriptSession.cpp",
             "RuntimeUiSession.cpp", "RuntimeEvidenceSession.cpp",
             "RuntimeFrameLoop.cpp"})
    {
        EXPECT_TRUE(ContainsCMakeToken(manifest, source)) << source;
    }

    EXPECT_NE(host.find("RunRuntimeFrameLoop"), std::string::npos);
    for (const auto* implementationDetail : {
             "Json::parse", "CaptureCurrentColorFrame", "LoadDocument",
             "CreateRmlUiUiBackend", "CSharpScriptHost", "ofstream"})
    {
        EXPECT_EQ(host.find(implementationDetail), std::string::npos)
            << implementationDetail;
    }

    EXPECT_NE(launch.find("Runtime.Envelope.SchemaInvalid"), std::string::npos);
    EXPECT_EQ(frameLoop.find("Runtime.Envelope.SchemaInvalid"), std::string::npos);
    EXPECT_NE(artifacts.find("RuntimeLaunchSession::VerifyFile"), std::string::npos);
    EXPECT_EQ(frameLoop.find("RuntimeLaunchSession::VerifyFile"), std::string::npos);
    EXPECT_NE(render.find("backend->Submit"), std::string::npos);
    EXPECT_EQ(ui.find("RenderView"), std::string::npos);
    EXPECT_NE(script.find("InvokeNamedAction"), std::string::npos);
    EXPECT_NE(evidence.find("CaptureCurrentColorFrame"), std::string::npos);
    EXPECT_EQ(evidence.find("SetBool"), std::string::npos);
    EXPECT_EQ(evidence.find("InvokeNamedAction"), std::string::npos);
    EXPECT_EQ(frameLoop.find("P6\\n"), std::string::npos);
}

TEST(CMakeTargetBoundaryTests, ReadOnlyProjectionStaysOutsideRuntimeLaunchAndExecution)
{
    const auto root = std::filesystem::path(SAGA_SOURCE_ROOT);
    const auto targets = ReadText(root / "cmake/modules/SagaTargets.cmake");
    const auto play = ReadText(root /
        "Engine/Source/Editor/EditorAuthoring/Private/SagaEditor/Play/PlayPreparationPipeline.cpp");
    const auto launch = ReadText(root /
        "Engine/Source/Runtime/SagaRuntime/Public/SagaRuntime/Play/RuntimePlayContracts.hpp");
    const auto qtPanel = ReadText(root /
        "Engine/Source/Editor/EditorQt/Private/SagaEditorQt/Panels/ScriptingPanel.cpp");
    const auto qtBlocks = ReadText(root /
        "Engine/Source/Editor/EditorQt/Private/SagaEditorQt/Panels/ReadOnlyBlocksWidget.cpp");
    const auto projectionHeader = ReadText(root /
        "Engine/Source/Editor/VisualBlocksEditor/Public/SagaEditor/VisualBlocks/ReadOnlyScriptProjection.h");
    const auto projectionSource = ReadText(root /
        "Engine/Source/Editor/VisualBlocksEditor/Private/SagaEditor/VisualBlocks/ReadOnlyScriptProjection.cpp");

    const auto runtimeLinks = ExtractTargetCalls(targets, "target_link_libraries", "SagaRuntime");
    EXPECT_EQ(runtimeLinks.find("VisualBlocks"), std::string::npos);
    EXPECT_EQ(launch.find("projection"), std::string::npos);
    EXPECT_EQ(launch.find("Projection"), std::string::npos);
    EXPECT_EQ(play.find("block-projection"), std::string::npos);
    EXPECT_EQ(play.find("ReadOnlyScriptProjection"), std::string::npos);
    EXPECT_EQ(qtPanel.find("Microsoft.CodeAnalysis"), std::string::npos);
    EXPECT_EQ(qtPanel.find("CSharpSyntaxTree"), std::string::npos);
    for (const auto* mutation : {"ReplaceText", "SourceDocumentTransaction",
                                 "ApplyPatch", "WriteFile"})
        EXPECT_EQ(qtBlocks.find(mutation), std::string::npos) << mutation;
    for (const auto& text : {projectionHeader, projectionSource})
    {
        EXPECT_EQ(text.find("VirtualMachine"), std::string::npos);
        EXPECT_EQ(text.find("ExecutionLoop"), std::string::npos);
        EXPECT_EQ(text.find("<fstream>"), std::string::npos);
    }
}

TEST(CMakeTargetBoundaryTests, ModuleTestDirectoriesContainRealOwnedTests)
{
    const auto root = std::filesystem::path(SAGA_SOURCE_ROOT);
    const std::array moduleGroups = {
        root / "Engine/Source/Runtime",
        root / "Engine/Source/Editor",
    };

    std::vector<std::filesystem::path> missingTests;
    std::vector<std::filesystem::path> placeholders;
    for (const auto& group : moduleGroups)
    {
        for (const auto& module : std::filesystem::directory_iterator(group))
        {
            if (!module.is_directory()) continue;
            const auto tests = module.path() / "Tests";
            if (!std::filesystem::is_directory(tests)) continue;

            bool hasSource = false;
            for (const auto& entry : std::filesystem::recursive_directory_iterator(tests))
            {
                if (!entry.is_regular_file()) continue;
                if (entry.path().filename() == ".gitkeep")
                    placeholders.push_back(entry.path());
                if (entry.path().extension() == ".cpp")
                    hasSource = true;
            }
            if (!hasSource) missingTests.push_back(tests);
        }
    }

    EXPECT_TRUE(missingTests.empty())
        << "Every declared module Tests directory needs an executable test source";
    EXPECT_TRUE(placeholders.empty())
        << "Populated module Tests directories must not retain .gitkeep files";
    EXPECT_FALSE(std::filesystem::exists(root / "Tools/SagaScript/Tests/.gitkeep"));
    EXPECT_FALSE(std::filesystem::exists(root / "Tools/SagaPackager/Tests/.gitkeep"));
    EXPECT_FALSE(std::filesystem::exists(root / "Tools/SagaScript/Public/.gitkeep"));
    EXPECT_FALSE(std::filesystem::exists(root / "Tools/SagaScript/Private/.gitkeep"));
    EXPECT_FALSE(std::filesystem::exists(root / "Tools/SagaPackager/Public/.gitkeep"));
    EXPECT_FALSE(std::filesystem::exists(root / "Tools/SagaPackager/Private/.gitkeep"));
}
