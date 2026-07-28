// SPDX-License-Identifier: MPL-2.0

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

TEST(ManagedToolRuntimeBoundaryTests, ProductToolsHaveOnePrivateRuntimeAuthority)
{
    const auto root = std::filesystem::path(SAGA_SOURCE_ROOT);
    const auto managedRuntime = root /
        "Engine/Source/Editor/EditorCore/Private/SagaEditor/Processes/ManagedToolRuntime.cpp";
    ASSERT_TRUE(std::filesystem::is_regular_file(managedRuntime));

    std::vector<std::filesystem::path> violations;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(
             root / "Engine/Source"))
    {
        if (!entry.is_regular_file() || entry.path() == managedRuntime ||
            entry.path().string().find("/Tests/") != std::string::npos)
            continue;
        const auto extension = entry.path().extension().string();
        if (extension != ".cpp" && extension != ".h" && extension != ".hpp")
            continue;
        const auto text = ReadText(entry.path());
        for (const auto* forbidden : {
                 "/usr/share/dotnet", "/nix/store",
                 "sagaProjectExecutable", "sagaScriptExecutable",
                 "sagaPackagerExecutable", "SAGA_DOTNET"})
        {
            if (text.find(forbidden) != std::string::npos)
                violations.push_back(entry.path());
        }
    }

    for (const auto& path : {
             root / "Tools/SagaPackager/package_linux/dotnet_tools.py",
             root / "Tools/Developer/DistributionCheck/smoke-linux-saga-dist",
             root / "Tools/SagaProjectKit/sagaproject",
             root / "Tools/SagaScript/sagascript",
             root / "Tools/SagaPackager/sagapack"})
    {
        const auto text = ReadText(path);
        for (const auto* forbidden : {
                 "/usr/share/dotnet", "/nix/store", "SAGA_DOTNET",
                 "shutil.which(\"dotnet\")", "dotnet run", "exec dotnet"})
        {
            if (text.find(forbidden) != std::string::npos)
                violations.push_back(path);
        }
    }

    EXPECT_TRUE(violations.empty())
        << "Production managed tools must resolve only through ManagedToolRuntime";
}
