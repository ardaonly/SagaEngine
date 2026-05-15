/// @file SharedBoundaryTests.cpp
/// @brief Unit checks for SagaShared public header dependency boundaries.

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace
{

std::string ReadFile(const std::filesystem::path& path)
{
    std::ifstream input(path);
    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

} // namespace

TEST(SharedBoundaryTests, PublicHeadersDoNotIncludeUpperLayers)
{
    const std::filesystem::path sharedRoot =
        std::filesystem::path(SAGA_SOURCE_ROOT) / "Shared" / "include";
    ASSERT_TRUE(std::filesystem::exists(sharedRoot));

    const std::vector<std::string> forbiddenTokens = {
        "SagaEditor/",
        "SagaServer/",
        "SagaCollaboration/",
        "Apps/Saga/",
        "SDE/",
        "<Q",
    };

    for (const auto& entry : std::filesystem::recursive_directory_iterator(sharedRoot))
    {
        if (!entry.is_regular_file())
        {
            continue;
        }
        if (entry.path().extension() != ".hpp" && entry.path().extension() != ".h")
        {
            continue;
        }

        const std::string text = ReadFile(entry.path());
        for (const std::string& token : forbiddenTokens)
        {
            EXPECT_EQ(text.find(token), std::string::npos)
                << entry.path().string() << " contains forbidden token " << token;
        }
    }
}
