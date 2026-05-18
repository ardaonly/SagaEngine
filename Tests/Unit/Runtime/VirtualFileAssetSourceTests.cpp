/// @file VirtualFileAssetSourceTests.cpp
/// @brief Unit tests for the VFS-backed runtime asset source adapter.

#include <SagaEngine/Resources/AssetRegistry.h>
#include <SagaEngine/Resources/VirtualFileAssetSource.h>
#include <SagaEngine/Resources/VirtualFileSystem.h>

#include <gtest/gtest.h>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace
{

using SagaEngine::Resources::AssetId;
using SagaEngine::Resources::AssetKind;
using SagaEngine::Resources::AssetRegistry;
using SagaEngine::Resources::AssetRegistryEntry;
using SagaEngine::Resources::MemoryVirtualFileSystemBackend;
using SagaEngine::Resources::NativeDirectoryVirtualFileSystemBackend;
using SagaEngine::Resources::StreamingRequest;
using SagaEngine::Resources::StreamingRequestHandle;
using SagaEngine::Resources::StreamingStatus;
using SagaEngine::Resources::VirtualFileAssetSource;
using SagaEngine::Resources::VirtualFileSystem;
using SagaEngine::Resources::VirtualFileSystemMountStatus;
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

[[nodiscard]] StreamingRequest MakeRequest(
    AssetId assetId,
    AssetKind kind,
    std::uint8_t lod = 0)
{
    StreamingRequest request;
    request.assetId = assetId;
    request.kind = kind;
    request.requestedLod = lod;
    return request;
}

[[nodiscard]] AssetRegistryEntry MakeRegistryEntry(
    AssetId assetId,
    std::string assetKey,
    AssetKind kind,
    std::string sourcePath)
{
    AssetRegistryEntry entry;
    entry.assetId = assetId;
    entry.assetKey = std::move(assetKey);
    entry.kind = kind;
    entry.sourcePath = std::move(sourcePath);
    entry.diskSizeBytes = 64;
    return entry;
}

[[nodiscard]] std::shared_ptr<VirtualFileSystem> MakeVfsWithMemoryFile(
    std::string path,
    const std::vector<std::uint8_t>& bytes)
{
    auto backend = std::make_shared<MemoryVirtualFileSystemBackend>();
    const auto writeResult = backend->SetFile(std::move(path), bytes);
    EXPECT_EQ(writeResult.status, VirtualFileSystemWriteStatus::Ok);

    auto vfs = std::make_shared<VirtualFileSystem>();
    const auto mountResult = vfs->Mount("/content", backend);
    EXPECT_EQ(mountResult.status, VirtualFileSystemMountStatus::Ok);
    return vfs;
}

} // namespace

TEST(VirtualFileAssetSourceTests, MemoryVfsReadPublishesPayloadMetadata)
{
    const std::vector<std::uint8_t> expectedBytes = {1, 2, 3, 4};
    auto vfs = MakeVfsWithMemoryFile("textures/hero.bin", expectedBytes);

    VirtualFileAssetSource source(
        vfs,
        [](AssetId assetId, AssetKind kind) {
            if (assetId == 2001 && kind == AssetKind::Texture)
            {
                return std::string("/content/textures/hero.bin");
            }
            return std::string{};
        });

    StreamingRequestHandle handle;
    const auto result = source.LoadBlocking(
        MakeRequest(2001, AssetKind::Texture, 2),
        handle);

    ASSERT_EQ(result.status, StreamingStatus::Ok);
    EXPECT_EQ(result.payload.kind, AssetKind::Texture);
    EXPECT_EQ(result.payload.lod, 2u);
    EXPECT_EQ(result.payload.sizeBytes, expectedBytes.size());
    EXPECT_EQ(result.payload.bytes, expectedBytes);
}

TEST(VirtualFileAssetSourceTests, NativeDirectoryVfsReadsExistingFile)
{
    const auto root = ResetTempDirectory("saga_virtual_file_asset_source_native");
    const std::vector<std::uint8_t> expectedBytes = {9, 8, 7};
    WriteBinaryFile(root / "meshes" / "tree.mesh", expectedBytes);

    auto vfs = std::make_shared<VirtualFileSystem>();
    auto backend =
        std::make_shared<NativeDirectoryVirtualFileSystemBackend>(root);
    ASSERT_EQ(
        vfs->Mount("/content", backend).status,
        VirtualFileSystemMountStatus::Ok);

    VirtualFileAssetSource source(
        vfs,
        [](AssetId assetId, AssetKind kind) {
            if (assetId == 2002 && kind == AssetKind::Mesh)
            {
                return std::string("/content/meshes/tree.mesh");
            }
            return std::string{};
        });

    StreamingRequestHandle handle;
    const auto result = source.LoadBlocking(
        MakeRequest(2002, AssetKind::Mesh),
        handle);

    ASSERT_EQ(result.status, StreamingStatus::Ok);
    EXPECT_EQ(result.payload.bytes, expectedBytes);
}

TEST(VirtualFileAssetSourceTests, EmptyResolverPathReturnsAssetNotFound)
{
    auto vfs = MakeVfsWithMemoryFile("asset.bin", {1});
    VirtualFileAssetSource source(
        vfs,
        [](AssetId, AssetKind) {
            return std::string{};
        });

    StreamingRequestHandle handle;
    const auto result = source.LoadBlocking(
        MakeRequest(2003, AssetKind::Texture),
        handle);

    EXPECT_EQ(result.status, StreamingStatus::AssetNotFound);
    EXPECT_TRUE(result.payload.bytes.empty());
    EXPECT_EQ(result.payload.sizeBytes, 0u);
    EXPECT_NE(result.diagnostic.find("empty path"), std::string::npos);
}

TEST(VirtualFileAssetSourceTests, MissingFileMapsToAssetNotFound)
{
    auto vfs = std::make_shared<VirtualFileSystem>();
    auto backend = std::make_shared<MemoryVirtualFileSystemBackend>();
    ASSERT_EQ(
        vfs->Mount("/content", backend).status,
        VirtualFileSystemMountStatus::Ok);

    VirtualFileAssetSource source(
        vfs,
        [](AssetId, AssetKind) {
            return std::string("/content/missing.bin");
        });

    StreamingRequestHandle handle;
    const auto result = source.LoadBlocking(
        MakeRequest(2004, AssetKind::Texture),
        handle);

    EXPECT_EQ(result.status, StreamingStatus::AssetNotFound);
    EXPECT_TRUE(result.payload.bytes.empty());
    EXPECT_NE(result.diagnostic.find("missing.bin"), std::string::npos);
}

TEST(VirtualFileAssetSourceTests, MountNotFoundMapsToAssetNotFound)
{
    auto vfs = std::make_shared<VirtualFileSystem>();
    VirtualFileAssetSource source(
        vfs,
        [](AssetId, AssetKind) {
            return std::string("/missing/asset.bin");
        });

    StreamingRequestHandle handle;
    const auto result = source.LoadBlocking(
        MakeRequest(2005, AssetKind::Texture),
        handle);

    EXPECT_EQ(result.status, StreamingStatus::AssetNotFound);
    EXPECT_TRUE(result.payload.bytes.empty());
    EXPECT_NE(result.diagnostic.find("/missing/asset.bin"), std::string::npos);
}

TEST(VirtualFileAssetSourceTests, InvalidVirtualPathMapsToSourceError)
{
    auto vfs = MakeVfsWithMemoryFile("asset.bin", {1});
    VirtualFileAssetSource source(
        vfs,
        [](AssetId, AssetKind) {
            return std::string("relative/asset.bin");
        });

    StreamingRequestHandle handle;
    const auto result = source.LoadBlocking(
        MakeRequest(2006, AssetKind::Texture),
        handle);

    EXPECT_EQ(result.status, StreamingStatus::SourceError);
    EXPECT_TRUE(result.payload.bytes.empty());
    EXPECT_NE(result.diagnostic.find("relative/asset.bin"), std::string::npos);
}

TEST(VirtualFileAssetSourceTests, NullResolverReturnsSourceError)
{
    auto vfs = MakeVfsWithMemoryFile("asset.bin", {1});
    VirtualFileAssetSource source(vfs, {});

    StreamingRequestHandle handle;
    const auto result = source.LoadBlocking(
        MakeRequest(2007, AssetKind::Texture),
        handle);

    EXPECT_EQ(result.status, StreamingStatus::SourceError);
    EXPECT_TRUE(result.payload.bytes.empty());
    EXPECT_NE(result.diagnostic.find("null path resolver"), std::string::npos);
}

TEST(VirtualFileAssetSourceTests, NullVfsReturnsSourceError)
{
    VirtualFileAssetSource source(
        nullptr,
        [](AssetId, AssetKind) {
            return std::string("/content/asset.bin");
        });

    StreamingRequestHandle handle;
    const auto result = source.LoadBlocking(
        MakeRequest(2008, AssetKind::Texture),
        handle);

    EXPECT_EQ(result.status, StreamingStatus::SourceError);
    EXPECT_TRUE(result.payload.bytes.empty());
    EXPECT_NE(result.diagnostic.find("null VFS"), std::string::npos);
}

TEST(VirtualFileAssetSourceTests, CancellationBeforeReadReturnsCancelled)
{
    auto vfs = MakeVfsWithMemoryFile("asset.bin", {1});
    VirtualFileAssetSource source(
        vfs,
        [](AssetId, AssetKind) {
            return std::string("/content/asset.bin");
        });

    StreamingRequestHandle handle;
    handle.Cancel();
    const auto result = source.LoadBlocking(
        MakeRequest(2009, AssetKind::Texture),
        handle);

    EXPECT_EQ(result.status, StreamingStatus::Cancelled);
    EXPECT_TRUE(result.payload.bytes.empty());
}

TEST(VirtualFileAssetSourceTests, FailureDoesNotMutateRegistryState)
{
    auto vfs = std::make_shared<VirtualFileSystem>();
    auto backend = std::make_shared<MemoryVirtualFileSystemBackend>();
    ASSERT_EQ(
        vfs->Mount("/content", backend).status,
        VirtualFileSystemMountStatus::Ok);

    AssetRegistry registry;
    const auto insertResult = registry.TryInsert(MakeRegistryEntry(
        2010,
        "texture.hero.diffuse",
        AssetKind::Texture,
        "/content/missing.ktx2"));
    ASSERT_TRUE(insertResult.Succeeded()) << insertResult.message;

    VirtualFileAssetSource source(
        vfs,
        [&registry](AssetId assetId, AssetKind kind) {
            const AssetRegistryEntry* entry = registry.Find(assetId);
            if (!entry || entry->kind != kind)
            {
                return std::string{};
            }
            return entry->sourcePath;
        });

    StreamingRequestHandle handle;
    const auto result = source.LoadBlocking(
        MakeRequest(2010, AssetKind::Texture),
        handle);

    EXPECT_EQ(result.status, StreamingStatus::AssetNotFound);
    EXPECT_TRUE(result.payload.bytes.empty());

    EXPECT_EQ(registry.EntryCount(), 1u);
    const AssetRegistryEntry* byId = registry.Find(2010);
    ASSERT_NE(byId, nullptr);
    EXPECT_EQ(byId->assetKey, "texture.hero.diffuse");
    EXPECT_EQ(byId->sourcePath, "/content/missing.ktx2");

    const AssetRegistryEntry* byKey =
        registry.FindByKey("texture.hero.diffuse");
    ASSERT_NE(byKey, nullptr);
    EXPECT_EQ(byKey->assetId, 2010u);
}
