#include <gtest/gtest.h>

#include <array>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace
{
bool IsHeader(const std::filesystem::path& path)
{
    const auto extension = path.extension().string();
    return extension == ".h" || extension == ".hh" ||
           extension == ".hpp" || extension == ".hxx";
}

std::string ReadText(const std::filesystem::path& path)
{
    std::ifstream input(path);
    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}
}

TEST(PublicPrivateBoundaryTests, CanonicalPublicOwnersExist)
{
    const auto root = std::filesystem::path(SAGA_SOURCE_ROOT);
    const std::array owners = {
        root / "Engine/Source/Runtime/Networking/Public/SagaEngine/Networking",
        root / "Engine/Source/Runtime/Replication/Public/SagaEngine/Replication",
        root / "Engine/Source/Runtime/Persistence/Public/SagaEngine/Persistence",
        root / "Engine/Source/Runtime/UI/Public/SagaEngine/UI/Backends",
        root / "Engine/Source/Editor/EditorQt/Public/SagaEditorQt",
        root / "Engine/Source/Editor/VisualBlocksEditor/Public/SagaEditor/VisualBlocks",
    };
    for (const auto& owner : owners)
    {
        EXPECT_TRUE(std::filesystem::is_directory(owner)) << owner;
    }
}

TEST(PublicPrivateBoundaryTests, QtIsOwnedOnlyByEditorQt)
{
    const auto editorRoot = std::filesystem::path(SAGA_SOURCE_ROOT) / "Engine/Source/Editor";
    std::vector<std::filesystem::path> offenders;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(editorRoot))
    {
        if (!entry.is_regular_file() || !IsHeader(entry.path()) ||
            entry.path().string().find("/Public/") == std::string::npos ||
            entry.path().string().find("/EditorQt/") != std::string::npos)
        {
            continue;
        }
        const auto text = ReadText(entry.path());
        if (text.find("#include <Q") != std::string::npos ||
            text.find("#include <Qt") != std::string::npos)
        {
            offenders.push_back(entry.path());
        }
    }
    EXPECT_TRUE(offenders.empty()) << "Qt includes leaked outside EditorQt public ownership";
}

TEST(PublicPrivateBoundaryTests, RuntimePublicHeadersDoNotExposeVendorNamespaces)
{
    const auto runtimeRoot = std::filesystem::path(SAGA_SOURCE_ROOT) / "Engine/Source/Runtime";
    std::vector<std::filesystem::path> offenders;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(runtimeRoot))
    {
        if (!entry.is_regular_file() || !IsHeader(entry.path()) ||
            entry.path().string().find("/Public/") == std::string::npos)
        {
            continue;
        }
        const auto text = ReadText(entry.path());
        if (text.find("Diligent::") != std::string::npos ||
            text.find("Rml::") != std::string::npos ||
            text.find("asio::") != std::string::npos)
        {
            offenders.push_back(entry.path());
        }
    }
    EXPECT_TRUE(offenders.empty()) << "A vendor namespace leaked through a runtime public header";
}
