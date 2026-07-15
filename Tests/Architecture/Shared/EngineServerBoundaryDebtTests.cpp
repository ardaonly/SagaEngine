#include <gtest/gtest.h>

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
}

TEST(EngineServerBoundaryTests, NetworkingDoesNotOwnAuthorityTypes)
{
    const auto networkingRoot =
        std::filesystem::path(SAGA_SOURCE_ROOT) / "Engine/Source/Runtime/Networking";
    std::vector<std::filesystem::path> offenders;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(networkingRoot))
    {
        if (!entry.is_regular_file())
        {
            continue;
        }
        const auto text = ReadText(entry.path());
        if (text.find("ActorOwnershipRegistry") != std::string::npos ||
            text.find("AuthoritativeMovement") != std::string::npos ||
            text.find("ShardManager") != std::string::npos ||
            text.find("ZoneServer") != std::string::npos)
        {
            offenders.push_back(entry.path());
        }
    }
    EXPECT_TRUE(offenders.empty()) << "Authority implementation leaked into Networking";
}
