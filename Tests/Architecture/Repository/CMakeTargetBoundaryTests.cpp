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
    const auto tests = ReadText(root / "cmake/modules/SagaTests.cmake");

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

TEST(CMakeTargetBoundaryTests, LegacyOwnershipRootsStayAbsent)
{
    const auto root = std::filesystem::path(SAGA_SOURCE_ROOT);
    for (const auto& legacy : std::initializer_list<std::string>{
             "Runtime", "Server", "Backends", "Collaboration", "Shared",
             "Apps", "Content", "core", "samples", std::string("Tools") + "/scripts"})
    {
        EXPECT_FALSE(std::filesystem::exists(root / legacy)) << legacy;
    }
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
             root / "Tests/Unit/Runtime/RenderClientAppTests.cpp"})
    {
        EXPECT_FALSE(std::filesystem::exists(placeholder)) << placeholder;
    }
}
