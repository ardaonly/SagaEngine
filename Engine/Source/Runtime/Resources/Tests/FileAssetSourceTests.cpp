/// @file FileAssetSourceTests.cpp
/// @brief Unit tests for filesystem-backed runtime asset source loading.

#include <SagaEngine/Resources/AssetRegistry.h>
#include <SagaEngine/Resources/FileAssetSource.h>

#include <gtest/gtest.h>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

namespace
{

using SagaEngine::Resources::AssetId;
using SagaEngine::Resources::AssetKind;
using SagaEngine::Resources::AssetRegistry;
using SagaEngine::Resources::AssetRegistryEntry;
using SagaEngine::Resources::FileAssetSource;
using SagaEngine::Resources::StreamingRequest;
using SagaEngine::Resources::StreamingRequestHandle;
using SagaEngine::Resources::StreamingStatus;

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

} // namespace

TEST(FileAssetSourceTests, ExistingFileLoadsBytes)
{
    const auto root = ResetTempDirectory("saga_file_asset_source_existing");
    const std::vector<std::uint8_t> expectedBytes = {0, 1, 2, 42, 255};
    WriteBinaryFile(root / "Textures" / "hero.bin", expectedBytes);

    FileAssetSource source(
        root.string(),
        [](AssetId assetId, AssetKind kind) {
            if (assetId == 1001 && kind == AssetKind::Texture)
            {
                return std::string("Textures/hero.bin");
            }
            return std::string{};
        });

    StreamingRequestHandle handle;
    const auto result = source.LoadBlocking(
        MakeRequest(1001, AssetKind::Texture),
        handle);

    ASSERT_EQ(result.status, StreamingStatus::Ok);
    EXPECT_EQ(result.payload.bytes, expectedBytes);
}

TEST(FileAssetSourceTests, MissingFileReturnsAssetNotFound)
{
    const auto root = ResetTempDirectory("saga_file_asset_source_missing");
    FileAssetSource source(
        root.string(),
        [](AssetId, AssetKind) {
            return std::string("Textures/missing.bin");
        });

    StreamingRequestHandle handle;
    const auto result = source.LoadBlocking(
        MakeRequest(1002, AssetKind::Texture),
        handle);

    EXPECT_EQ(result.status, StreamingStatus::AssetNotFound);
    EXPECT_TRUE(result.payload.bytes.empty());
    EXPECT_EQ(result.payload.sizeBytes, 0u);
    EXPECT_NE(result.diagnostic.find("cannot open"), std::string::npos);
}

TEST(FileAssetSourceTests, EmptyResolverPathReturnsDeterministicAssetNotFound)
{
    const auto root = ResetTempDirectory("saga_file_asset_source_empty_path");
    FileAssetSource source(
        root.string(),
        [](AssetId, AssetKind) {
            return std::string{};
        });

    StreamingRequestHandle handle;
    const auto result = source.LoadBlocking(
        MakeRequest(1003, AssetKind::Mesh),
        handle);

    EXPECT_EQ(result.status, StreamingStatus::AssetNotFound);
    EXPECT_TRUE(result.payload.bytes.empty());
    EXPECT_EQ(result.payload.sizeBytes, 0u);
    EXPECT_NE(result.diagnostic.find("empty path"), std::string::npos);
    EXPECT_NE(result.diagnostic.find("1003"), std::string::npos);
}

TEST(FileAssetSourceTests, SuccessfulReadSetsPayloadMetadata)
{
    const auto root = ResetTempDirectory("saga_file_asset_source_metadata");
    const std::vector<std::uint8_t> expectedBytes = {9, 8, 7, 6};
    WriteBinaryFile(root / "Meshes" / "tree.mesh", expectedBytes);

    FileAssetSource source(
        root.string(),
        [](AssetId assetId, AssetKind kind) {
            if (assetId == 1004 && kind == AssetKind::Mesh)
            {
                return std::string("Meshes/tree.mesh");
            }
            return std::string{};
        });

    StreamingRequestHandle handle;
    const auto result = source.LoadBlocking(
        MakeRequest(1004, AssetKind::Mesh, 3),
        handle);

    ASSERT_EQ(result.status, StreamingStatus::Ok);
    EXPECT_EQ(result.payload.kind, AssetKind::Mesh);
    EXPECT_EQ(result.payload.lod, 3u);
    EXPECT_EQ(result.payload.sizeBytes, expectedBytes.size());
    EXPECT_EQ(result.payload.bytes, expectedBytes);
}

TEST(FileAssetSourceTests, MissingFileDoesNotMutateRegistryState)
{
    const auto root = ResetTempDirectory("saga_file_asset_source_registry_state");

    AssetRegistry registry;
    const auto insertResult = registry.TryInsert(MakeRegistryEntry(
        1005,
        "texture.hero.diffuse",
        AssetKind::Texture,
        "Textures/missing.ktx2"));
    ASSERT_TRUE(insertResult.Succeeded()) << insertResult.message;

    FileAssetSource source(
        root.string(),
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
        MakeRequest(1005, AssetKind::Texture),
        handle);

    EXPECT_EQ(result.status, StreamingStatus::AssetNotFound);
    EXPECT_TRUE(result.payload.bytes.empty());
    EXPECT_EQ(result.payload.sizeBytes, 0u);

    EXPECT_EQ(registry.EntryCount(), 1u);
    const AssetRegistryEntry* byId = registry.Find(1005);
    ASSERT_NE(byId, nullptr);
    EXPECT_EQ(byId->assetKey, "texture.hero.diffuse");
    EXPECT_EQ(byId->sourcePath, "Textures/missing.ktx2");

    const AssetRegistryEntry* byKey =
        registry.FindByKey("texture.hero.diffuse");
    ASSERT_NE(byKey, nullptr);
    EXPECT_EQ(byKey->assetId, 1005u);
}
