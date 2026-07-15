/// @file SagaPublicScriptingApiTests.cpp
/// @brief Compile and boundary tests for the public Saga scripting API.

#include <SagaEngine/Scripting.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
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

std::vector<std::filesystem::path> LayeredPublicHeaders()
{
    const auto root = std::filesystem::path(SAGA_SOURCE_ROOT) /
        "Engine" / "Source" / "Runtime" / "Scripting" / "Public" / "SagaEngine";
    std::vector<std::filesystem::path> headers = {
        root / "Scripting.hpp",
        root / "Scripting" / "Namespace.hpp",
        root / "Scripting" / "LowLevel.hpp",
        root / "Scripting" / "Authoring.hpp",
    };

    for (const auto& folder : {
             root / "Scripting" / "LowLevel",
             root / "Scripting" / "Authoring",
         })
    {
        for (const auto& entry :
             std::filesystem::recursive_directory_iterator(folder))
        {
            if (entry.is_regular_file() && entry.path().extension() == ".hpp")
            {
                headers.push_back(entry.path());
            }
        }
    }

    return headers;
}

bool ContainsAny(
    const std::string& text,
    const std::vector<std::string>& tokens)
{
    return std::any_of(
        tokens.begin(),
        tokens.end(),
        [&text](const std::string& token) {
            return text.find(token) != std::string::npos;
        });
}

} // namespace

TEST(SagaPublicScriptingApiTests, UmbrellaExposesLowLevelAndAuthoringTypes)
{
    Saga::Scripting::ScriptPackageHandle package{42};
    Saga::Scripting::ScriptInstanceCreateRequest instanceRequest;
    instanceRequest.package = package;
    instanceRequest.classId.value = "Game.PlayerController";
    instanceRequest.scriptId = "script://game/player";
    instanceRequest.selfEntity.value = 7;

    Saga::Scripting::ScriptBindingManifestLoadRequest manifestRequest;
    Saga::Scripting::ScriptBindingRuntimeCreateRequest runtimeRequest;
    runtimeRequest.packageRequest.packageRoot = manifestRequest.packageRoot;

    Saga::Scripting::ISagaScriptHost* host = nullptr;
    Saga::Scripting::ScriptLifecycleService* lifecycle = nullptr;

    EXPECT_TRUE(package.IsValid());
    EXPECT_TRUE(instanceRequest.selfEntity.IsValid());
    EXPECT_NE(instanceRequest.classId.value.find("Player"), std::string::npos);
    EXPECT_EQ(host, nullptr);
    EXPECT_EQ(lifecycle, nullptr);
}

TEST(SagaPublicScriptingApiTests, LayeredHeadersDoNotExposeConcreteRuntimeDependencies)
{
    const std::vector<std::string> forbidden = {
        "hostfxr",
        "HostFxr",
        "nethost",
        "CoreCLR",
        "Microsoft.CodeAnalysis",
        "Roslyn",
        "Qt",
        "SagaEditor",
        "WorldState",
        "SagaEngine/ECS",
        "ECS/",
    };

    for (const auto& header : LayeredPublicHeaders())
    {
        ASSERT_TRUE(std::filesystem::exists(header)) << header.generic_string();
        EXPECT_FALSE(ContainsAny(ReadText(header), forbidden))
            << "Forbidden dependency token found in " << header.generic_string();
    }
}

TEST(SagaPublicScriptingApiTests, LowLevelHeadersDoNotIncludeAuthoring)
{
    const auto root = std::filesystem::path(SAGA_SOURCE_ROOT) /
        "Engine" / "Source" / "Runtime" / "Scripting" / "Public" / "SagaEngine" / "Scripting";
    std::vector<std::filesystem::path> lowLevelHeaders = {
        root / "LowLevel.hpp",
    };

    for (const auto& entry :
         std::filesystem::recursive_directory_iterator(root / "LowLevel"))
    {
        if (entry.is_regular_file() && entry.path().extension() == ".hpp")
        {
            lowLevelHeaders.push_back(entry.path());
        }
    }

    for (const auto& header : lowLevelHeaders)
    {
        ASSERT_TRUE(std::filesystem::exists(header)) << header.generic_string();
        EXPECT_EQ(ReadText(header).find("Authoring"), std::string::npos)
            << "Low-level header includes authoring surface: "
            << header.generic_string();
    }
}
