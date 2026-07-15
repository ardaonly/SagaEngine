#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

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

TEST(SharedBoundaryTests, SharedContractsStayIndependentOfUpperLayers)
{
    const auto sourceRoot = std::filesystem::path(SAGA_SOURCE_ROOT) / "Engine/Source";
    std::size_t contractRoots = 0;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(sourceRoot))
    {
        if (!entry.is_directory() || entry.path().filename() != "SagaShared" ||
            entry.path().parent_path().filename() != "Public")
        {
            continue;
        }
        ++contractRoots;
        for (const auto& contract : std::filesystem::recursive_directory_iterator(entry.path()))
        {
            if (!contract.is_regular_file())
            {
                continue;
            }
            const auto text = ReadText(contract.path());
            EXPECT_EQ(text.find("SagaEditor/"), std::string::npos) << contract.path();
            EXPECT_EQ(text.find("SagaEngine/ServerAuthority/"), std::string::npos) << contract.path();
            EXPECT_EQ(text.find("#include <Q"), std::string::npos) << contract.path();
        }
    }
    EXPECT_GT(contractRoots, 0u);
}
