/// @file PublicPrivateBoundaryTests.cpp
/// @brief Guards Public and Private include boundaries for module migration.

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace
{

bool IsCodeFile(const std::filesystem::path& path)
{
    const auto ext = path.extension().string();
    return ext == ".h" || ext == ".hh" || ext == ".hpp" || ext == ".hxx" ||
           ext == ".c" || ext == ".cc" || ext == ".cpp" || ext == ".cxx";
}

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

std::filesystem::path RelativeToSourceRoot(const std::filesystem::path& path)
{
    return std::filesystem::relative(path, std::filesystem::path(SAGA_SOURCE_ROOT));
}

} // namespace

TEST(PublicPrivateBoundaryTests, EngineModulesDoNotExposePrivateIncludes)
{
    const auto root = std::filesystem::path(SAGA_SOURCE_ROOT);
    const auto engineRoot = root / "Engine";
    ASSERT_TRUE(std::filesystem::exists(engineRoot / "Public"));
    ASSERT_TRUE(std::filesystem::exists(engineRoot / "Private"));
    EXPECT_FALSE(std::filesystem::exists(root / "Source" / "SagaEngine"));
    EXPECT_FALSE(std::filesystem::exists(engineRoot / "include"));
    EXPECT_FALSE(std::filesystem::exists(engineRoot / "src"));

    std::vector<std::filesystem::path> layoutOffenders;
    for (const auto& entry : std::filesystem::directory_iterator(engineRoot))
    {
        if (!entry.is_directory())
        {
            continue;
        }

        const auto name = entry.path().filename().string();
        if (name == "Public" || name == "Private")
        {
            continue;
        }

        if (std::filesystem::exists(entry.path() / "Public") ||
            std::filesystem::exists(entry.path() / "Private"))
        {
            layoutOffenders.push_back(RelativeToSourceRoot(entry.path()));
        }
    }

    EXPECT_TRUE(layoutOffenders.empty())
        << "Only Engine/Public and Engine/Private may own the public/private split. "
        << "First offender: "
        << (layoutOffenders.empty() ? "" : layoutOffenders.front().generic_string());

    std::vector<std::string> offenders;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(root))
    {
        if (!entry.is_regular_file() || !IsCodeFile(entry.path()))
        {
            continue;
        }

        const auto relative = RelativeToSourceRoot(entry.path());
        const auto relText = relative.generic_string();
        const auto lines = ReadLines(entry.path());
        for (std::size_t i = 0; i < lines.size(); ++i)
        {
            const auto& line = lines[i];
            if (line.find("#include") == std::string::npos ||
                line.find("Private/") == std::string::npos)
            {
                continue;
            }

            offenders.push_back(
                relText + ":" + std::to_string(i + 1) + ": " + line);
        }
    }

    EXPECT_TRUE(offenders.empty())
        << "Code must not include paths containing Private/. First offender: "
        << (offenders.empty() ? "" : offenders.front());
}

TEST(PublicPrivateBoundaryTests, DiagnosticsPublicHeadersDoNotDependUpward)
{
    const auto root = std::filesystem::path(SAGA_SOURCE_ROOT);
    const auto diagnosticsPublic =
        root / "Engine" / "Public" / "SagaEngine" / "Diagnostics";
    ASSERT_TRUE(std::filesystem::exists(diagnosticsPublic));

    std::vector<std::filesystem::path> offenders;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(diagnosticsPublic))
    {
        if (!entry.is_regular_file() || !IsCodeFile(entry.path()))
        {
            continue;
        }

        const auto text = ReadText(entry.path());
        if (text.find("SagaServer/") != std::string::npos ||
            text.find("SagaEditor/") != std::string::npos ||
            text.find("SDE/") != std::string::npos ||
            text.find("SagaEngine/Server/") != std::string::npos ||
            text.find("SagaEngine/Editor/") != std::string::npos ||
            text.find("SagaEngine/Core/Private/") != std::string::npos)
        {
            offenders.push_back(RelativeToSourceRoot(entry.path()));
        }
    }

    EXPECT_TRUE(offenders.empty())
        << "SagaDiagnostics public headers must not depend on Server, Editor, or SDE.";
}

TEST(PublicPrivateBoundaryTests, EnginePublicDoesNotExposeServerOrConcreteBackends)
{
    const auto root = std::filesystem::path(SAGA_SOURCE_ROOT);
    const auto enginePublic = root / "Engine" / "Public";
    ASSERT_TRUE(std::filesystem::exists(enginePublic));

    EXPECT_FALSE(std::filesystem::exists(enginePublic / "SagaEngine" / "Server"))
        << "Server policy belongs under root Server/, not Engine/Public.";
    EXPECT_FALSE(std::filesystem::exists(enginePublic / "SagaEngine" / "Platform" / "SDL"))
        << "Concrete SDL platform classes must stay private behind PlatformFactory.";
    EXPECT_FALSE(std::filesystem::exists(enginePublic / "SagaEngine" / "Input" / "Backends" / "SDL"))
        << "Concrete SDL input classes must stay private behind IPlatformInputBackend factories.";
    EXPECT_FALSE(std::filesystem::exists(enginePublic / "SagaEngine" / "Render" / "Backend" / "Diligent"))
        << "Concrete render backend classes must stay private behind RenderBackendFactory.";
    EXPECT_FALSE(std::filesystem::exists(enginePublic / "SagaEngine" / "Resources" / "Formats" / "GltfMeshImporter.h"))
        << "glTF mesh import is a resource implementation detail, not public engine API.";

    const std::vector<std::string> forbiddenIncludes = {
        "SagaEngine/Server/",
        "SagaEngine/Platform/SDL/",
        "SagaEngine/Input/Backends/SDL/",
        "SagaEngine/Render/Backend/Diligent/",
    };

    std::vector<std::string> offenders;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(enginePublic))
    {
        if (!entry.is_regular_file() || !IsCodeFile(entry.path()))
        {
            continue;
        }

        const auto relative = RelativeToSourceRoot(entry.path()).generic_string();
        const auto lines = ReadLines(entry.path());
        for (std::size_t i = 0; i < lines.size(); ++i)
        {
            const auto& line = lines[i];
            if (line.find("#include") == std::string::npos)
            {
                continue;
            }

            for (const auto& forbidden : forbiddenIncludes)
            {
                if (line.find(forbidden) != std::string::npos)
                {
                    offenders.push_back(
                        relative + ":" + std::to_string(i + 1) + ": " + line);
                }
            }
        }
    }

    EXPECT_TRUE(offenders.empty())
        << "Engine public headers must not expose fixed server/backend paths. First offender: "
        << (offenders.empty() ? "" : offenders.front());
}

TEST(PublicPrivateBoundaryTests, SimulationPublicDoesNotIncludeInputNetworking)
{
    const auto root = std::filesystem::path(SAGA_SOURCE_ROOT);
    const auto simulationPublic = root / "Engine" / "Public" / "SagaEngine" / "Simulation";
    ASSERT_TRUE(std::filesystem::exists(simulationPublic));

    std::vector<std::string> offenders;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(simulationPublic))
    {
        if (!entry.is_regular_file() || !IsCodeFile(entry.path()))
        {
            continue;
        }

        const auto relative = RelativeToSourceRoot(entry.path()).generic_string();
        const auto lines = ReadLines(entry.path());
        for (std::size_t i = 0; i < lines.size(); ++i)
        {
            const auto& line = lines[i];
            if (line.find("#include") != std::string::npos &&
                line.find("SagaEngine/Input/Networking/") != std::string::npos)
            {
                offenders.push_back(
                    relative + ":" + std::to_string(i + 1) + ": " + line);
            }
        }
    }

    EXPECT_TRUE(offenders.empty())
        << "Simulation public headers must not depend on input networking. First offender: "
        << (offenders.empty() ? "" : offenders.front());
}
