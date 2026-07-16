/// @file ArtifactStartupValidatorTests.cpp
/// @brief Unit tests for runtime artifact startup acceptance policy.

#include <SagaEngine/Artifacts/ArtifactStartupValidator.hpp>

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

namespace
{

using SagaEngine::Artifacts::ArtifactManifestDiagnostics::DuplicateId;
using SagaEngine::Artifacts::ArtifactManifestDiagnostics::FileMissing;
using SagaEngine::Artifacts::ArtifactManifestDiagnostics::InvalidField;
using SagaEngine::Artifacts::ArtifactManifestDiagnostics::ManifestMissing;
using SagaEngine::Artifacts::ArtifactManifestDiagnostics::MissingField;
using SagaEngine::Artifacts::ArtifactStartupValidator;

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

[[nodiscard]] std::filesystem::path WriteManifest(
    const char* directoryName,
    const char* contents)
{
    const auto root = TempArtifactDirectory(directoryName);
    const auto manifestPath = root / "artifact_manifest.json";
    WriteTempFile(manifestPath, contents);
    return manifestPath;
}

} // namespace

TEST(ArtifactStartupValidatorTests, ValidManifestWithExistingArtifactSucceeds)
{
    const auto manifestPath = WriteManifest(
        "saga_startup_valid_manifest",
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
    WriteTempFile(manifestPath.parent_path() / "Build" / "Artifacts" / "Graphs" /
                      "quest_reward.graph.json",
                  "{}");

    const auto result = ArtifactStartupValidator::ValidateManifestForStartup(manifestPath);

    ASSERT_TRUE(result.Succeeded());
    EXPECT_EQ(result.artifactRoot, manifestPath.parent_path().lexically_normal());
    ASSERT_EQ(result.manifest.artifacts.size(), 1u);
    EXPECT_EQ(result.manifest.artifacts[0].id, "quest_reward_graph");
}

TEST(ArtifactStartupValidatorTests, MissingManifestPropagatesManifestMissing)
{
    const auto manifestPath =
        TempArtifactDirectory("saga_startup_missing_manifest") / "missing.json";

    const auto result = ArtifactStartupValidator::ValidateManifestForStartup(manifestPath);

    ASSERT_FALSE(result.Succeeded());
    ASSERT_EQ(result.errors.size(), 1u);
    EXPECT_EQ(result.errors[0].diagnosticId, ManifestMissing);
}

TEST(ArtifactStartupValidatorTests, LoaderFailurePropagatesLoaderDiagnostic)
{
    const auto manifestPath = WriteManifest(
        "saga_startup_loader_failure",
        R"({
  "schemaVersion": 1
})");

    const auto result = ArtifactStartupValidator::ValidateManifestForStartup(manifestPath);

    ASSERT_FALSE(result.Succeeded());
    ASSERT_EQ(result.errors.size(), 1u);
    EXPECT_EQ(result.errors[0].diagnosticId, MissingField);
    ASSERT_TRUE(result.errors[0].fieldName.has_value());
    EXPECT_EQ(*result.errors[0].fieldName, "artifacts");
}

TEST(ArtifactStartupValidatorTests, EmptyArtifactIdFailsStartupPolicy)
{
    const auto manifestPath = WriteManifest(
        "saga_startup_empty_id",
        R"({
  "schemaVersion": 1,
  "artifacts": [
    {
      "id": "",
      "kind": "graph",
      "path": "Build/Artifacts/Graphs/quest_reward.graph.json"
    }
  ]
})");
    WriteTempFile(manifestPath.parent_path() / "Build" / "Artifacts" / "Graphs" /
                      "quest_reward.graph.json",
                  "{}");

    const auto result = ArtifactStartupValidator::ValidateManifestForStartup(manifestPath);

    ASSERT_FALSE(result.Succeeded());
    ASSERT_EQ(result.errors.size(), 1u);
    EXPECT_EQ(result.errors[0].diagnosticId, InvalidField);
    EXPECT_EQ(result.errors[0].fieldName, "id");
}

TEST(ArtifactStartupValidatorTests, DuplicateArtifactIdsFailStartupPolicy)
{
    const auto manifestPath = WriteManifest(
        "saga_startup_duplicate_ids",
        R"({
  "schemaVersion": 1,
  "artifacts": [
    {
      "id": "quest_reward_graph",
      "kind": "graph",
      "path": "Build/Artifacts/Graphs/quest_reward_a.graph.json"
    },
    {
      "id": "quest_reward_graph",
      "kind": "graph",
      "path": "Build/Artifacts/Graphs/quest_reward_b.graph.json"
    }
  ]
})");
    WriteTempFile(manifestPath.parent_path() / "Build" / "Artifacts" / "Graphs" /
                      "quest_reward_a.graph.json",
                  "{}");
    WriteTempFile(manifestPath.parent_path() / "Build" / "Artifacts" / "Graphs" /
                      "quest_reward_b.graph.json",
                  "{}");

    const auto result = ArtifactStartupValidator::ValidateManifestForStartup(manifestPath);

    ASSERT_FALSE(result.Succeeded());
    ASSERT_EQ(result.errors.size(), 1u);
    EXPECT_EQ(result.errors[0].diagnosticId, DuplicateId);
    EXPECT_EQ(result.errors[0].artifactId, "quest_reward_graph");
    EXPECT_EQ(result.errors[0].artifactIndex, 1u);
}

TEST(ArtifactStartupValidatorTests, EmptyArtifactPathFailsStartupPolicy)
{
    const auto manifestPath = WriteManifest(
        "saga_startup_empty_path",
        R"({
  "schemaVersion": 1,
  "artifacts": [
    {
      "id": "quest_reward_graph",
      "kind": "graph",
      "path": ""
    }
  ]
})");

    const auto result = ArtifactStartupValidator::ValidateManifestForStartup(manifestPath);

    ASSERT_FALSE(result.Succeeded());
    ASSERT_EQ(result.errors.size(), 1u);
    EXPECT_EQ(result.errors[0].diagnosticId, InvalidField);
    EXPECT_EQ(result.errors[0].fieldName, "path");
}

TEST(ArtifactStartupValidatorTests, AbsoluteArtifactPathFailsStartupPolicy)
{
    const auto manifestPath = WriteManifest(
        "saga_startup_absolute_path",
        R"({
  "schemaVersion": 1,
  "artifacts": [
    {
      "id": "quest_reward_graph",
      "kind": "graph",
      "path": "/tmp/quest_reward.graph.json"
    }
  ]
})");

    const auto result = ArtifactStartupValidator::ValidateManifestForStartup(manifestPath);

    ASSERT_FALSE(result.Succeeded());
    ASSERT_EQ(result.errors.size(), 1u);
    EXPECT_EQ(result.errors[0].diagnosticId, InvalidField);
    EXPECT_EQ(result.errors[0].fieldName, "path");
}

TEST(ArtifactStartupValidatorTests, EscapingArtifactPathFailsStartupPolicy)
{
    const auto manifestPath = WriteManifest(
        "saga_startup_escaping_path",
        R"({
  "schemaVersion": 1,
  "artifacts": [
    {
      "id": "quest_reward_graph",
      "kind": "graph",
      "path": "../outside.graph.json"
    }
  ]
})");

    const auto result = ArtifactStartupValidator::ValidateManifestForStartup(manifestPath);

    ASSERT_FALSE(result.Succeeded());
    ASSERT_EQ(result.errors.size(), 1u);
    EXPECT_EQ(result.errors[0].diagnosticId, InvalidField);
    EXPECT_EQ(result.errors[0].fieldName, "path");
}

TEST(ArtifactStartupValidatorTests, MissingReferencedArtifactFileReturnsFileMissing)
{
    const auto manifestPath = WriteManifest(
        "saga_startup_missing_artifact_file",
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

    const auto result = ArtifactStartupValidator::ValidateManifestForStartup(manifestPath);

    ASSERT_FALSE(result.Succeeded());
    ASSERT_EQ(result.errors.size(), 1u);
    EXPECT_EQ(result.errors[0].diagnosticId, FileMissing);
    EXPECT_EQ(result.errors[0].artifactId, "quest_reward_graph");
    ASSERT_TRUE(result.errors[0].resolvedPath.has_value());
}

TEST(ArtifactStartupValidatorTests, RelativeArtifactPathResolvesAgainstManifestParent)
{
    const auto manifestPath = WriteManifest(
        "saga_startup_manifest_parent_root",
        R"({
  "schemaVersion": 1,
  "artifacts": [
    {
      "id": "quest_reward_graph",
      "kind": "graph",
      "path": "quest_reward.graph.json"
    }
  ]
})");
    WriteTempFile(manifestPath.parent_path() / "quest_reward.graph.json", "{}");

    const auto result = ArtifactStartupValidator::ValidateManifestForStartup(manifestPath);

    EXPECT_TRUE(result.Succeeded());
    EXPECT_EQ(result.artifactRoot, manifestPath.parent_path().lexically_normal());
}
