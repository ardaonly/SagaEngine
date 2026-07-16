/// @file ArtifactManifestLoaderTests.cpp
/// @brief Unit tests for runtime artifact manifest loading and validation.

#include <SagaEngine/Artifacts/ArtifactManifestLoader.hpp>

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

namespace
{

using SagaEngine::Artifacts::ArtifactKind;
using SagaEngine::Artifacts::ArtifactManifestDiagnostics::FileMissing;
using SagaEngine::Artifacts::ArtifactManifestDiagnostics::InvalidField;
using SagaEngine::Artifacts::ArtifactManifestDiagnostics::ManifestMissing;
using SagaEngine::Artifacts::ArtifactManifestDiagnostics::MissingField;
using SagaEngine::Artifacts::ArtifactManifestDiagnostics::UnknownKind;
using SagaEngine::Artifacts::ArtifactManifestDiagnostics::UnsupportedVersion;
using SagaEngine::Artifacts::ArtifactManifestLoader;
using SagaEngine::Artifacts::ArtifactManifestLoadOptions;

[[nodiscard]] std::filesystem::path FixturePath(const char* name)
{
    return std::filesystem::path(SAGA_SOURCE_ROOT) /
           "Tests" / "Fixtures" / "Assets" / "ArtifactManifests" / name;
}

[[nodiscard]] std::filesystem::path WriteTempManifest(const char* name, const char* contents)
{
    const auto path = std::filesystem::temp_directory_path() / name;
    std::ofstream output(path);
    output << contents;
    return path;
}

[[nodiscard]] std::filesystem::path TempArtifactDirectory(const char* name)
{
    const auto path = std::filesystem::temp_directory_path() / name;
    std::filesystem::create_directories(path);
    return path;
}

void WriteTempFile(const std::filesystem::path& path, const char* contents)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream output(path);
    output << contents;
}

} // namespace

TEST(ArtifactManifestLoaderTests, ValidManifestLoadsArtifactReferences)
{
    const auto result = ArtifactManifestLoader::LoadFromFile(
        FixturePath("valid_artifact_manifest.json"));

    ASSERT_TRUE(result.Succeeded());
    EXPECT_EQ(result.manifest.schemaVersion, 1u);
    ASSERT_EQ(result.manifest.artifacts.size(), 1u);
    EXPECT_EQ(result.manifest.artifacts[0].id, "quest_reward_graph");
    EXPECT_EQ(result.manifest.artifacts[0].kind, ArtifactKind::Graph);
    EXPECT_EQ(result.manifest.artifacts[0].path,
              "Build/Artifacts/Graphs/quest_reward.graph.json");
    ASSERT_TRUE(result.manifest.artifacts[0].hash.has_value());
    EXPECT_EQ(*result.manifest.artifacts[0].hash, "optional-for-now");
}

TEST(ArtifactManifestLoaderTests, MissingFileReturnsManifestMissing)
{
    const auto result = ArtifactManifestLoader::LoadFromFile(
        FixturePath("does_not_exist.json"));

    ASSERT_FALSE(result.Succeeded());
    ASSERT_EQ(result.errors.size(), 1u);
    EXPECT_EQ(result.errors[0].diagnosticId, ManifestMissing);
}

TEST(ArtifactManifestLoaderTests, InvalidJsonReturnsParseFailed)
{
    const auto path = WriteTempManifest(
        "saga_artifact_manifest_invalid_json.json",
        R"({
  "schemaVersion": 1,
  "artifacts": [
})");

    const auto result = ArtifactManifestLoader::LoadFromFile(path);

    ASSERT_FALSE(result.Succeeded());
    ASSERT_EQ(result.errors.size(), 1u);
    EXPECT_EQ(result.errors[0].diagnosticId,
              SagaEngine::Artifacts::ArtifactManifestDiagnostics::ParseFailed);
}

TEST(ArtifactManifestLoaderTests, NonObjectRootReturnsParseFailed)
{
    const auto path = WriteTempManifest(
        "saga_artifact_manifest_non_object_root.json",
        R"([
  {
    "schemaVersion": 1
  }
])");

    const auto result = ArtifactManifestLoader::LoadFromFile(path);

    ASSERT_FALSE(result.Succeeded());
    ASSERT_EQ(result.errors.size(), 1u);
    EXPECT_EQ(result.errors[0].diagnosticId,
              SagaEngine::Artifacts::ArtifactManifestDiagnostics::ParseFailed);
}

TEST(ArtifactManifestLoaderTests, MissingSchemaVersionReturnsMissingField)
{
    const auto path = WriteTempManifest(
        "saga_artifact_manifest_missing_schema_version.json",
        R"({
  "artifacts": []
})");

    const auto result = ArtifactManifestLoader::LoadFromFile(path);

    ASSERT_FALSE(result.Succeeded());
    ASSERT_EQ(result.errors.size(), 1u);
    EXPECT_EQ(result.errors[0].diagnosticId, MissingField);
    ASSERT_TRUE(result.errors[0].fieldName.has_value());
    EXPECT_EQ(*result.errors[0].fieldName, "schemaVersion");
}

TEST(ArtifactManifestLoaderTests, NonIntegerSchemaVersionReturnsInvalidField)
{
    const auto path = WriteTempManifest(
        "saga_artifact_manifest_non_integer_schema_version.json",
        R"({
  "schemaVersion": "1",
  "artifacts": []
})");

    const auto result = ArtifactManifestLoader::LoadFromFile(path);

    ASSERT_FALSE(result.Succeeded());
    ASSERT_EQ(result.errors.size(), 1u);
    EXPECT_EQ(result.errors[0].diagnosticId, InvalidField);
    ASSERT_TRUE(result.errors[0].fieldName.has_value());
    EXPECT_EQ(*result.errors[0].fieldName, "schemaVersion");
}

TEST(ArtifactManifestLoaderTests, UnsupportedVersionReturnsUnsupportedVersion)
{
    const auto path = WriteTempManifest(
        "saga_artifact_manifest_unsupported_version.json",
        R"({
  "schemaVersion": 2,
  "artifacts": []
})");

    const auto result = ArtifactManifestLoader::LoadFromFile(path);

    ASSERT_FALSE(result.Succeeded());
    ASSERT_EQ(result.errors.size(), 1u);
    EXPECT_EQ(result.errors[0].diagnosticId, UnsupportedVersion);
    ASSERT_TRUE(result.errors[0].fieldName.has_value());
    EXPECT_EQ(*result.errors[0].fieldName, "schemaVersion");
}

TEST(ArtifactManifestLoaderTests, MissingArtifactsReturnsMissingField)
{
    const auto path = WriteTempManifest(
        "saga_artifact_manifest_missing_artifacts.json",
        R"({
  "schemaVersion": 1
})");

    const auto result = ArtifactManifestLoader::LoadFromFile(path);

    ASSERT_FALSE(result.Succeeded());
    ASSERT_EQ(result.errors.size(), 1u);
    EXPECT_EQ(result.errors[0].diagnosticId, MissingField);
    ASSERT_TRUE(result.errors[0].fieldName.has_value());
    EXPECT_EQ(*result.errors[0].fieldName, "artifacts");
}

TEST(ArtifactManifestLoaderTests, NonArrayArtifactsReturnsInvalidField)
{
    const auto path = WriteTempManifest(
        "saga_artifact_manifest_non_array_artifacts.json",
        R"({
  "schemaVersion": 1,
  "artifacts": {}
})");

    const auto result = ArtifactManifestLoader::LoadFromFile(path);

    ASSERT_FALSE(result.Succeeded());
    ASSERT_EQ(result.errors.size(), 1u);
    EXPECT_EQ(result.errors[0].diagnosticId, InvalidField);
    ASSERT_TRUE(result.errors[0].fieldName.has_value());
    EXPECT_EQ(*result.errors[0].fieldName, "artifacts");
}

TEST(ArtifactManifestLoaderTests, NonObjectArtifactEntryReturnsInvalidField)
{
    const auto path = WriteTempManifest(
        "saga_artifact_manifest_non_object_artifact_entry.json",
        R"({
  "schemaVersion": 1,
  "artifacts": ["not-an-object"]
})");

    const auto result = ArtifactManifestLoader::LoadFromFile(path);

    ASSERT_FALSE(result.Succeeded());
    ASSERT_EQ(result.errors.size(), 1u);
    EXPECT_EQ(result.errors[0].diagnosticId, InvalidField);
    ASSERT_TRUE(result.errors[0].artifactIndex.has_value());
    EXPECT_EQ(*result.errors[0].artifactIndex, 0u);
}

TEST(ArtifactManifestLoaderTests, MissingArtifactIdReturnsMissingField)
{
    const auto path = WriteTempManifest(
        "saga_artifact_manifest_missing_id.json",
        R"({
  "schemaVersion": 1,
  "artifacts": [
    {
      "kind": "graph",
      "path": "Build/Artifacts/Graphs/quest_reward.graph.json"
    }
  ]
})");

    const auto result = ArtifactManifestLoader::LoadFromFile(path);

    ASSERT_FALSE(result.Succeeded());
    ASSERT_EQ(result.errors.size(), 1u);
    EXPECT_EQ(result.errors[0].diagnosticId, MissingField);
    ASSERT_TRUE(result.errors[0].fieldName.has_value());
    EXPECT_EQ(*result.errors[0].fieldName, "id");
}

TEST(ArtifactManifestLoaderTests, NonStringArtifactIdReturnsInvalidField)
{
    const auto path = WriteTempManifest(
        "saga_artifact_manifest_non_string_id.json",
        R"({
  "schemaVersion": 1,
  "artifacts": [
    {
      "id": 7,
      "kind": "graph",
      "path": "Build/Artifacts/Graphs/quest_reward.graph.json"
    }
  ]
})");

    const auto result = ArtifactManifestLoader::LoadFromFile(path);

    ASSERT_FALSE(result.Succeeded());
    ASSERT_EQ(result.errors.size(), 1u);
    EXPECT_EQ(result.errors[0].diagnosticId, InvalidField);
    ASSERT_TRUE(result.errors[0].fieldName.has_value());
    EXPECT_EQ(*result.errors[0].fieldName, "id");
}

TEST(ArtifactManifestLoaderTests, MissingArtifactKindReturnsMissingField)
{
    const auto path = WriteTempManifest(
        "saga_artifact_manifest_missing_kind.json",
        R"({
  "schemaVersion": 1,
  "artifacts": [
    {
      "id": "quest_reward_graph",
      "path": "Build/Artifacts/Graphs/quest_reward.graph.json"
    }
  ]
})");

    const auto result = ArtifactManifestLoader::LoadFromFile(path);

    ASSERT_FALSE(result.Succeeded());
    ASSERT_EQ(result.errors.size(), 1u);
    EXPECT_EQ(result.errors[0].diagnosticId, MissingField);
    ASSERT_TRUE(result.errors[0].fieldName.has_value());
    EXPECT_EQ(*result.errors[0].fieldName, "kind");
}

TEST(ArtifactManifestLoaderTests, NonStringArtifactKindReturnsInvalidField)
{
    const auto path = WriteTempManifest(
        "saga_artifact_manifest_non_string_kind.json",
        R"({
  "schemaVersion": 1,
  "artifacts": [
    {
      "id": "quest_reward_graph",
      "kind": 7,
      "path": "Build/Artifacts/Graphs/quest_reward.graph.json"
    }
  ]
})");

    const auto result = ArtifactManifestLoader::LoadFromFile(path);

    ASSERT_FALSE(result.Succeeded());
    ASSERT_EQ(result.errors.size(), 1u);
    EXPECT_EQ(result.errors[0].diagnosticId, InvalidField);
    ASSERT_TRUE(result.errors[0].fieldName.has_value());
    EXPECT_EQ(*result.errors[0].fieldName, "kind");
}

TEST(ArtifactManifestLoaderTests, MissingArtifactPathReturnsMissingField)
{
    const auto result = ArtifactManifestLoader::LoadFromFile(
        FixturePath("invalid_missing_path_manifest.json"));

    ASSERT_FALSE(result.Succeeded());
    ASSERT_EQ(result.errors.size(), 1u);
    EXPECT_EQ(result.errors[0].diagnosticId, MissingField);
    ASSERT_TRUE(result.errors[0].artifactIndex.has_value());
    EXPECT_EQ(*result.errors[0].artifactIndex, 0u);
    ASSERT_TRUE(result.errors[0].fieldName.has_value());
    EXPECT_EQ(*result.errors[0].fieldName, "path");
}

TEST(ArtifactManifestLoaderTests, NonStringArtifactPathReturnsInvalidField)
{
    const auto path = WriteTempManifest(
        "saga_artifact_manifest_non_string_path.json",
        R"({
  "schemaVersion": 1,
  "artifacts": [
    {
      "id": "quest_reward_graph",
      "kind": "graph",
      "path": 7
    }
  ]
})");

    const auto result = ArtifactManifestLoader::LoadFromFile(path);

    ASSERT_FALSE(result.Succeeded());
    ASSERT_EQ(result.errors.size(), 1u);
    EXPECT_EQ(result.errors[0].diagnosticId, InvalidField);
    ASSERT_TRUE(result.errors[0].fieldName.has_value());
    EXPECT_EQ(*result.errors[0].fieldName, "path");
}

TEST(ArtifactManifestLoaderTests, UnknownKindReturnsUnknownKind)
{
    const auto path = WriteTempManifest(
        "saga_artifact_manifest_unknown_kind.json",
        R"({
  "schemaVersion": 1,
  "artifacts": [
    {
      "id": "quest_reward_graph",
      "kind": "mystery",
      "path": "Build/Artifacts/Graphs/quest_reward.graph.json"
    }
  ]
})");

    const auto result = ArtifactManifestLoader::LoadFromFile(path);

    ASSERT_FALSE(result.Succeeded());
    ASSERT_EQ(result.errors.size(), 1u);
    EXPECT_EQ(result.errors[0].diagnosticId, UnknownKind);
    ASSERT_TRUE(result.errors[0].fieldName.has_value());
    EXPECT_EQ(*result.errors[0].fieldName, "kind");
}

TEST(ArtifactManifestLoaderTests, OptionalHashStringIsAccepted)
{
    const auto path = WriteTempManifest(
        "saga_artifact_manifest_optional_hash.json",
        R"({
  "schemaVersion": 1,
  "artifacts": [
    {
      "id": "quest_reward_graph",
      "kind": "graph",
      "path": "Build/Artifacts/Graphs/quest_reward.graph.json",
      "hash": "sha256:abc"
    }
  ]
})");

    const auto result = ArtifactManifestLoader::LoadFromFile(path);

    ASSERT_TRUE(result.Succeeded());
    ASSERT_EQ(result.manifest.artifacts.size(), 1u);
    ASSERT_TRUE(result.manifest.artifacts[0].hash.has_value());
    EXPECT_EQ(*result.manifest.artifacts[0].hash, "sha256:abc");
}

TEST(ArtifactManifestLoaderTests, NonStringHashReturnsInvalidField)
{
    const auto path = WriteTempManifest(
        "saga_artifact_manifest_non_string_hash.json",
        R"({
  "schemaVersion": 1,
  "artifacts": [
    {
      "id": "quest_reward_graph",
      "kind": "graph",
      "path": "Build/Artifacts/Graphs/quest_reward.graph.json",
      "hash": 7
    }
  ]
})");

    const auto result = ArtifactManifestLoader::LoadFromFile(path);

    ASSERT_FALSE(result.Succeeded());
    ASSERT_EQ(result.errors.size(), 1u);
    EXPECT_EQ(result.errors[0].diagnosticId, InvalidField);
    ASSERT_TRUE(result.errors[0].fieldName.has_value());
    EXPECT_EQ(*result.errors[0].fieldName, "hash");
}

TEST(ArtifactManifestLoaderTests, DefaultOptionsDoNotValidateArtifactFileExistence)
{
    const auto path = WriteTempManifest(
        "saga_artifact_manifest_unvalidated_missing_file.json",
        R"({
  "schemaVersion": 1,
  "artifacts": [
    {
      "id": "quest_reward_graph",
      "kind": "graph",
      "path": "Build/Artifacts/Graphs/missing.graph.json"
    }
  ]
})");

    const auto result = ArtifactManifestLoader::LoadFromFile(path);

    ASSERT_TRUE(result.Succeeded());
    ASSERT_EQ(result.manifest.artifacts.size(), 1u);
    EXPECT_EQ(result.manifest.artifacts[0].path,
              "Build/Artifacts/Graphs/missing.graph.json");
}

TEST(ArtifactManifestLoaderTests, ValidationSucceedsWhenArtifactFileExistsUnderBaseDirectory)
{
    const auto baseDirectory =
        TempArtifactDirectory("saga_artifact_manifest_existing_base");
    WriteTempFile(baseDirectory / "Build" / "Artifacts" / "Graphs" / "quest_reward.graph.json",
                  "{}");
    const auto manifestPath = WriteTempManifest(
        "saga_artifact_manifest_existing_base.json",
        R"({
  "schemaVersion": 1,
  "artifacts": [
    {
      "id": "quest_reward_graph",
      "kind": "graph",
      "path": "Build/Artifacts/Graphs/quest_reward.graph.json"
    }
  ]
})");

    ArtifactManifestLoadOptions options;
    options.validateArtifactFiles = true;
    options.artifactBaseDirectory = baseDirectory;

    const auto result = ArtifactManifestLoader::LoadFromFile(manifestPath, options);

    EXPECT_TRUE(result.Succeeded());
    ASSERT_EQ(result.manifest.artifacts.size(), 1u);
    EXPECT_EQ(result.manifest.artifacts[0].id, "quest_reward_graph");
}

TEST(ArtifactManifestLoaderTests, ValidationReturnsFileMissingWhenArtifactFileIsAbsent)
{
    const auto baseDirectory =
        TempArtifactDirectory("saga_artifact_manifest_missing_base");
    const auto manifestPath = WriteTempManifest(
        "saga_artifact_manifest_missing_base.json",
        R"({
  "schemaVersion": 1,
  "artifacts": [
    {
      "id": "quest_reward_graph",
      "kind": "graph",
      "path": "Build/Artifacts/Graphs/missing.graph.json"
    }
  ]
})");

    ArtifactManifestLoadOptions options;
    options.validateArtifactFiles = true;
    options.artifactBaseDirectory = baseDirectory;

    const auto result = ArtifactManifestLoader::LoadFromFile(manifestPath, options);

    ASSERT_FALSE(result.Succeeded());
    ASSERT_EQ(result.errors.size(), 1u);
    EXPECT_EQ(result.errors[0].diagnosticId, FileMissing);
    ASSERT_TRUE(result.errors[0].artifactIndex.has_value());
    EXPECT_EQ(*result.errors[0].artifactIndex, 0u);
    ASSERT_TRUE(result.errors[0].fieldName.has_value());
    EXPECT_EQ(*result.errors[0].fieldName, "path");
    ASSERT_EQ(result.manifest.artifacts.size(), 1u);
}

TEST(ArtifactManifestLoaderTests, ValidationUsesManifestDirectoryWhenBaseDirectoryIsEmpty)
{
    const auto manifestDirectory =
        TempArtifactDirectory("saga_artifact_manifest_parent_base");
    WriteTempFile(manifestDirectory / "Build" / "Artifacts" / "Graphs" /
                      "quest_reward.graph.json",
                  "{}");
    const auto manifestPath = manifestDirectory / "artifact_manifest.json";
    WriteTempFile(manifestPath,
                  R"({
  "schemaVersion": 1,
  "artifacts": [
    {
      "id": "quest_reward_graph",
      "kind": "graph",
      "path": "Build/Artifacts/Graphs/quest_reward.graph.json"
    }
  ]
})");

    ArtifactManifestLoadOptions options;
    options.validateArtifactFiles = true;

    const auto result = ArtifactManifestLoader::LoadFromFile(manifestPath, options);

    EXPECT_TRUE(result.Succeeded());
    ASSERT_EQ(result.manifest.artifacts.size(), 1u);
    EXPECT_EQ(result.manifest.artifacts[0].kind, ArtifactKind::Graph);
}
