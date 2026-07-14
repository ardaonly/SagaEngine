/// @file EngineServerBoundaryDebtTests.cpp
/// @brief Guards known temporary Engine public-header SagaServer dependencies.

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <set>
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

std::filesystem::path RelativeToSourceRoot(const std::filesystem::path& path)
{
    return std::filesystem::relative(path, std::filesystem::path(SAGA_SOURCE_ROOT));
}

} // namespace

TEST(EngineServerBoundaryDebtTests, PublicHeaderLeaksStayOnTemporaryAllowlist)
{
    const std::filesystem::path enginePublic =
        std::filesystem::path(SAGA_SOURCE_ROOT) / "Engine" / "Public";
    ASSERT_TRUE(std::filesystem::exists(enginePublic));

    const std::set<std::filesystem::path> allowedLeaks;

    std::vector<std::filesystem::path> unexpectedLeaks;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(enginePublic))
    {
        if (!entry.is_regular_file())
        {
            continue;
        }

        const auto ext = entry.path().extension().string();
        if (ext != ".h" && ext != ".hpp" && ext != ".hh" && ext != ".hxx")
        {
            continue;
        }

        const std::string text = ReadText(entry.path());
        if (text.find("SagaServer/") == std::string::npos &&
            text.find("SagaServer::") == std::string::npos)
        {
            continue;
        }

        const std::filesystem::path relative = RelativeToSourceRoot(entry.path());
        if (allowedLeaks.find(relative) == allowedLeaks.end())
        {
            unexpectedLeaks.push_back(relative);
        }
    }

    EXPECT_TRUE(unexpectedLeaks.empty())
        << "Engine public headers must not add new SagaServer dependencies";
}
