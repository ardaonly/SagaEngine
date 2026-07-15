#include <gtest/gtest.h>

#include <array>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <sstream>
#include <string>
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
