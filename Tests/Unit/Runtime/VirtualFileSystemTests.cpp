/// @file VirtualFileSystemTests.cpp
/// @brief Unit tests for runtime virtual content path access.

#include <SagaEngine/Resources/VirtualFileSystem.h>

#include <gtest/gtest.h>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

namespace
{

using SagaEngine::Resources::MemoryVirtualFileSystemBackend;
using SagaEngine::Resources::NativeDirectoryVirtualFileSystemBackend;
using SagaEngine::Resources::VirtualFileSystem;
using SagaEngine::Resources::VirtualFileSystemMountStatus;
using SagaEngine::Resources::VirtualFileSystemStatus;
using SagaEngine::Resources::VirtualFileSystemWriteStatus;

[[nodiscard]] std::filesystem::path ResetTempDirectory(const char* name)
{
    const auto path = std::filesystem::temp_directory_path() / name;
    std::filesystem::remove_all(path);
    std::filesystem::create_directories(path);
    return path;
}

void WriteBinaryFile(
    const std::filesystem::path& path,
    const std::vector<std::uint8_t>& bytes)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary);
    output.write(
        reinterpret_cast<const char*>(bytes.data()),
        static_cast<std::streamsize>(bytes.size()));
}

[[nodiscard]] std::shared_ptr<MemoryVirtualFileSystemBackend> MakeMemoryBackend(
    const std::string& relativePath,
    const std::vector<std::uint8_t>& bytes)
{
    auto backend = std::make_shared<MemoryVirtualFileSystemBackend>();
    const auto writeResult = backend->SetFile(relativePath, bytes);
    EXPECT_EQ(writeResult.status, VirtualFileSystemWriteStatus::Ok);
    return backend;
}

} // namespace

TEST(VirtualFileSystemTests, MemoryBackendReadsNormalizedVirtualPath)
{
    const std::vector<std::uint8_t> expectedBytes = {1, 2, 3, 4};
    auto backend = MakeMemoryBackend("textures/hero.bin", expectedBytes);

    VirtualFileSystem vfs;
    const auto mountResult = vfs.Mount("/content", backend);
    ASSERT_EQ(mountResult.status, VirtualFileSystemMountStatus::Ok);

    const auto result = vfs.ReadFile("/content/textures/hero.bin");

    ASSERT_EQ(result.status, VirtualFileSystemStatus::Ok);
    EXPECT_EQ(result.bytes, expectedBytes);
}

TEST(VirtualFileSystemTests, NativeDirectoryBackendReadsExistingFile)
{
    const auto root = ResetTempDirectory("saga_vfs_native_existing");
    const std::vector<std::uint8_t> expectedBytes = {9, 8, 7};
    WriteBinaryFile(root / "meshes" / "tree.bin", expectedBytes);

    auto backend =
        std::make_shared<NativeDirectoryVirtualFileSystemBackend>(root);
    VirtualFileSystem vfs;
    const auto mountResult = vfs.Mount("/content", backend);
    ASSERT_EQ(mountResult.status, VirtualFileSystemMountStatus::Ok);

    const auto result = vfs.ReadFile("/content/meshes/tree.bin");

    ASSERT_EQ(result.status, VirtualFileSystemStatus::Ok);
    EXPECT_EQ(result.bytes, expectedBytes);
}

TEST(VirtualFileSystemTests, RootMountReadsThroughRootPrefix)
{
    const std::vector<std::uint8_t> expectedBytes = {4, 5, 6};
    auto backend = MakeMemoryBackend("asset.bin", expectedBytes);

    VirtualFileSystem vfs;
    const auto mountResult = vfs.Mount("/", backend);
    ASSERT_EQ(mountResult.status, VirtualFileSystemMountStatus::Ok);

    const auto result = vfs.ReadFile("/asset.bin");

    ASSERT_EQ(result.status, VirtualFileSystemStatus::Ok);
    EXPECT_EQ(result.bytes, expectedBytes);
}

TEST(VirtualFileSystemTests, ReadingMountPointItselfReturnsInvalidPath)
{
    auto backend = MakeMemoryBackend("asset.bin", {1});

    VirtualFileSystem vfs;
    const auto mountResult = vfs.Mount("/content", backend);
    ASSERT_EQ(mountResult.status, VirtualFileSystemMountStatus::Ok);

    const auto result = vfs.ReadFile("/content");

    EXPECT_EQ(result.status, VirtualFileSystemStatus::InvalidPath);
    EXPECT_TRUE(result.bytes.empty());
    EXPECT_NE(result.diagnostic.find("mount-relative"), std::string::npos);
}

TEST(VirtualFileSystemTests, MissingFileReturnsAssetNotFound)
{
    auto backend = std::make_shared<MemoryVirtualFileSystemBackend>();

    VirtualFileSystem vfs;
    const auto mountResult = vfs.Mount("/content", backend);
    ASSERT_EQ(mountResult.status, VirtualFileSystemMountStatus::Ok);

    const auto result = vfs.ReadFile("/content/missing.bin");

    EXPECT_EQ(result.status, VirtualFileSystemStatus::AssetNotFound);
    EXPECT_TRUE(result.bytes.empty());
    EXPECT_NE(result.diagnostic.find("missing.bin"), std::string::npos);
}

TEST(VirtualFileSystemTests, NativeDirectoryMissingFileReturnsAssetNotFound)
{
    const auto root = ResetTempDirectory("saga_vfs_native_missing");
    auto backend =
        std::make_shared<NativeDirectoryVirtualFileSystemBackend>(root);

    VirtualFileSystem vfs;
    const auto mountResult = vfs.Mount("/content", backend);
    ASSERT_EQ(mountResult.status, VirtualFileSystemMountStatus::Ok);

    const auto result = vfs.ReadFile("/content/missing.bin");

    EXPECT_EQ(result.status, VirtualFileSystemStatus::AssetNotFound);
    EXPECT_TRUE(result.bytes.empty());
    EXPECT_NE(result.diagnostic.find("missing.bin"), std::string::npos);
}

TEST(VirtualFileSystemTests, InvalidVirtualPathsFailDeterministically)
{
    auto backend = MakeMemoryBackend("valid.bin", {1});

    VirtualFileSystem vfs;
    const auto mountResult = vfs.Mount("/content", backend);
    ASSERT_EQ(mountResult.status, VirtualFileSystemMountStatus::Ok);

    const std::vector<std::string> invalidPaths = {
        "content/valid.bin",
        "/content\\valid.bin",
        "/content/../valid.bin",
        "/content//valid.bin"};

    for (const std::string& invalidPath : invalidPaths)
    {
        const auto result = vfs.ReadFile(invalidPath);
        EXPECT_EQ(result.status, VirtualFileSystemStatus::InvalidPath)
            << invalidPath;
        EXPECT_TRUE(result.bytes.empty()) << invalidPath;
        EXPECT_NE(result.diagnostic.find(invalidPath), std::string::npos)
            << invalidPath;
    }
}

TEST(VirtualFileSystemTests, InvalidMountPointsFailDeterministically)
{
    auto backend = MakeMemoryBackend("asset.bin", {1});

    VirtualFileSystem vfs;
    const std::vector<std::string> invalidMountPoints = {
        "content",
        "/content\\native",
        "/content/../native",
        "/content//native"};

    for (const std::string& invalidMountPoint : invalidMountPoints)
    {
        const auto result = vfs.Mount(invalidMountPoint, backend);
        EXPECT_EQ(
            result.status,
            VirtualFileSystemMountStatus::InvalidMountPoint)
            << invalidMountPoint;
        EXPECT_NE(
            result.diagnostic.find(invalidMountPoint),
            std::string::npos)
            << invalidMountPoint;
    }

    EXPECT_EQ(vfs.MountCount(), 0u);
}

TEST(VirtualFileSystemTests, NullBackendMountFailsDeterministically)
{
    VirtualFileSystem vfs;

    const auto result = vfs.Mount("/content", nullptr);

    EXPECT_EQ(result.status, VirtualFileSystemMountStatus::InvalidBackend);
    EXPECT_NE(result.diagnostic.find("/content"), std::string::npos);
    EXPECT_EQ(vfs.MountCount(), 0u);
}

TEST(VirtualFileSystemTests, UnknownMountReturnsMountNotFound)
{
    auto backend = MakeMemoryBackend("asset.bin", {1});

    VirtualFileSystem vfs;
    const auto mountResult = vfs.Mount("/content", backend);
    ASSERT_EQ(mountResult.status, VirtualFileSystemMountStatus::Ok);

    const auto result = vfs.ReadFile("/other/asset.bin");

    EXPECT_EQ(result.status, VirtualFileSystemStatus::MountNotFound);
    EXPECT_TRUE(result.bytes.empty());
    EXPECT_NE(result.diagnostic.find("/other/asset.bin"), std::string::npos);
}

TEST(VirtualFileSystemTests, MountPrefixDoesNotMatchPartialSegment)
{
    auto backend = MakeMemoryBackend("asset.bin", {1});

    VirtualFileSystem vfs;
    const auto mountResult = vfs.Mount("/content", backend);
    ASSERT_EQ(mountResult.status, VirtualFileSystemMountStatus::Ok);

    const auto result = vfs.ReadFile("/content2/asset.bin");

    EXPECT_EQ(result.status, VirtualFileSystemStatus::MountNotFound);
    EXPECT_TRUE(result.bytes.empty());
}

TEST(VirtualFileSystemTests, LongestMountPrefixWins)
{
    auto rootBackend = MakeMemoryBackend("dlc/asset.bin", {1});
    auto dlcBackend = MakeMemoryBackend("asset.bin", {2, 3});

    VirtualFileSystem vfs;
    ASSERT_EQ(
        vfs.Mount("/content", rootBackend).status,
        VirtualFileSystemMountStatus::Ok);
    ASSERT_EQ(
        vfs.Mount("/content/dlc", dlcBackend).status,
        VirtualFileSystemMountStatus::Ok);

    const auto result = vfs.ReadFile("/content/dlc/asset.bin");

    ASSERT_EQ(result.status, VirtualFileSystemStatus::Ok);
    EXPECT_EQ(result.bytes, std::vector<std::uint8_t>({2, 3}));
}

TEST(VirtualFileSystemTests, DuplicateMountFailsWithoutReplacingExistingMount)
{
    auto firstBackend = MakeMemoryBackend("asset.bin", {5});
    auto duplicateBackend = MakeMemoryBackend("asset.bin", {9});

    VirtualFileSystem vfs;
    ASSERT_EQ(
        vfs.Mount("/content", firstBackend).status,
        VirtualFileSystemMountStatus::Ok);

    const auto duplicateResult = vfs.Mount("/content", duplicateBackend);
    EXPECT_EQ(
        duplicateResult.status,
        VirtualFileSystemMountStatus::DuplicateMount);
    EXPECT_EQ(vfs.MountCount(), 1u);

    const auto result = vfs.ReadFile("/content/asset.bin");

    ASSERT_EQ(result.status, VirtualFileSystemStatus::Ok);
    EXPECT_EQ(result.bytes, std::vector<std::uint8_t>({5}));
}

TEST(VirtualFileSystemTests, MemoryBackendRejectsInvalidWritePath)
{
    MemoryVirtualFileSystemBackend backend;

    const auto result = backend.SetFile("../escape.bin", {1});

    EXPECT_EQ(result.status, VirtualFileSystemWriteStatus::InvalidPath);
    EXPECT_NE(result.diagnostic.find("../escape.bin"), std::string::npos);
    EXPECT_EQ(backend.FileCount(), 0u);
}

TEST(VirtualFileSystemTests, BackendsRejectInvalidRelativeReadPath)
{
    const auto root = ResetTempDirectory("saga_vfs_invalid_backend_read");
    MemoryVirtualFileSystemBackend memoryBackend;
    NativeDirectoryVirtualFileSystemBackend nativeBackend(root);

    const std::vector<std::string> invalidPaths = {
        "/absolute.bin",
        "../escape.bin",
        "folder//asset.bin",
        "folder\\asset.bin"};

    for (const std::string& invalidPath : invalidPaths)
    {
        const auto memoryResult = memoryBackend.ReadFile(invalidPath);
        EXPECT_EQ(memoryResult.status, VirtualFileSystemStatus::InvalidPath)
            << invalidPath;
        EXPECT_TRUE(memoryResult.bytes.empty()) << invalidPath;

        const auto nativeResult = nativeBackend.ReadFile(invalidPath);
        EXPECT_EQ(nativeResult.status, VirtualFileSystemStatus::InvalidPath)
            << invalidPath;
        EXPECT_TRUE(nativeResult.bytes.empty()) << invalidPath;
    }
}

TEST(VirtualFileSystemTests, FailedReadDoesNotMutateMountTableOrMemoryBackend)
{
    auto backend = MakeMemoryBackend("asset.bin", {7, 8});

    VirtualFileSystem vfs;
    ASSERT_EQ(
        vfs.Mount("/content", backend).status,
        VirtualFileSystemMountStatus::Ok);

    const auto missingResult = vfs.ReadFile("/content/missing.bin");
    EXPECT_EQ(missingResult.status, VirtualFileSystemStatus::AssetNotFound);
    EXPECT_EQ(vfs.MountCount(), 1u);
    EXPECT_EQ(backend->FileCount(), 1u);

    const auto existingResult = vfs.ReadFile("/content/asset.bin");
    ASSERT_EQ(existingResult.status, VirtualFileSystemStatus::Ok);
    EXPECT_EQ(existingResult.bytes, std::vector<std::uint8_t>({7, 8}));
}
