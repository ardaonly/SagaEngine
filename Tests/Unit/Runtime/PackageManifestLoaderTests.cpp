/// @file PackageManifestLoaderTests.cpp
/// @brief Unit tests for runtime package manifest loading and startup validation.

#include <SagaEngine/Packages/PackageManifestLoader.hpp>
#include <SagaEngine/Packages/PackageStartupValidator.hpp>

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

namespace
{

using SagaEngine::Packages::PackageKind;
using SagaEngine::Packages::PackageManifestDiagnostics::DuplicateId;
using SagaEngine::Packages::PackageManifestDiagnostics::FileMissing;
using SagaEngine::Packages::PackageManifestDiagnostics::InvalidPath;
using SagaEngine::Packages::PackageManifestDiagnostics::ManifestMissing;
using SagaEngine::Packages::PackageManifestDiagnostics::UnknownKind;
using SagaEngine::Packages::PackageManifestLoader;
using SagaEngine::Packages::PackageManifestLoadOptions;
using SagaEngine::Packages::PackageStartupDiagnostics::IncompatibleRuntime;
using SagaEngine::Packages::PackageStartupDiagnostics::WrongPackageKind;
using SagaEngine::Packages::PackageStartupValidationOptions;
using SagaEngine::Packages::PackageStartupValidator;

[[nodiscard]] std::filesystem::path TempPath(const char* name)
{
    return std::filesystem::temp_directory_path() / name;
}

std::filesystem::path WriteTempFile(
    const std::filesystem::path& path,
    const char* contents)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream output(path);
    output << contents;
    return path;
}

[[nodiscard]] const char* ValidPackageManifestJson()
{
    return R"({
  "schemaVersion": 1,
  "packageId": "starter.client",
  "packageKind": "client",
  "buildProfile": "dev-client",
  "targetPlatform": "linux",
  "runtimeCompatibilityVersion": "0.0.8",
  "assetManifests": [
    { "id": "assets.main", "path": "Manifests/assets.json" }
  ],
  "artifactManifests": [
    { "id": "artifacts.main", "path": "Manifests/artifacts.json" }
  ],
  "packageHash": "sha256-demo"
})";
}

} // namespace

TEST(PackageManifestLoaderTests, ValidManifestLoadsPackageMetadata)
{
    const auto path = WriteTempFile(
        TempPath("saga_package_manifest_valid.json"),
        ValidPackageManifestJson());

    const auto result = PackageManifestLoader::LoadFromFile(path);

    ASSERT_TRUE(result.Succeeded());
    EXPECT_EQ(result.manifest.schemaVersion, 1u);
    EXPECT_EQ(result.manifest.packageId, "starter.client");
    EXPECT_EQ(result.manifest.packageKind, PackageKind::Client);
    EXPECT_EQ(result.manifest.buildProfile, "dev-client");
    EXPECT_EQ(result.manifest.targetPlatform, "linux");
    EXPECT_EQ(result.manifest.runtimeCompatibilityVersion, "0.0.8");
    ASSERT_EQ(result.manifest.assetManifests.size(), 1u);
    EXPECT_EQ(result.manifest.assetManifests[0].path, "Manifests/assets.json");
    ASSERT_EQ(result.manifest.artifactManifests.size(), 1u);
    EXPECT_EQ(result.manifest.artifactManifests[0].path, "Manifests/artifacts.json");
}

TEST(PackageManifestLoaderTests, MissingFileReturnsManifestMissing)
{
    const auto result = PackageManifestLoader::LoadFromFile(
        TempPath("saga_package_manifest_missing.json"));

    ASSERT_FALSE(result.Succeeded());
    ASSERT_EQ(result.errors.size(), 1u);
    EXPECT_EQ(result.errors[0].diagnosticId, ManifestMissing);
}

TEST(PackageManifestLoaderTests, UnknownPackageKindReturnsUnknownKind)
{
    const auto path = WriteTempFile(
        TempPath("saga_package_manifest_unknown_kind.json"),
        R"({
  "schemaVersion": 1,
  "packageId": "starter.client",
  "packageKind": "editor",
  "buildProfile": "dev-client",
  "targetPlatform": "linux",
  "runtimeCompatibilityVersion": "0.0.8",
  "assetManifests": [],
  "artifactManifests": []
})");

    const auto result = PackageManifestLoader::LoadFromFile(path);

    ASSERT_FALSE(result.Succeeded());
    ASSERT_EQ(result.errors.size(), 1u);
    EXPECT_EQ(result.errors[0].diagnosticId, UnknownKind);
}

TEST(PackageManifestLoaderTests, DuplicateManifestRefsReturnDuplicateId)
{
    const auto path = WriteTempFile(
        TempPath("saga_package_manifest_duplicate_refs.json"),
        R"({
  "schemaVersion": 1,
  "packageId": "starter.client",
  "packageKind": "client",
  "buildProfile": "dev-client",
  "targetPlatform": "linux",
  "runtimeCompatibilityVersion": "0.0.8",
  "assetManifests": [
    { "id": "assets.main", "path": "Manifests/assets.json" },
    { "id": "assets.main", "path": "Manifests/other-assets.json" }
  ],
  "artifactManifests": []
})");

    const auto result = PackageManifestLoader::LoadFromFile(path);

    ASSERT_FALSE(result.Succeeded());
    ASSERT_EQ(result.errors.size(), 1u);
    EXPECT_EQ(result.errors[0].diagnosticId, DuplicateId);
}

TEST(PackageManifestLoaderTests, EscapingReferencePathReturnsInvalidPath)
{
    const auto path = WriteTempFile(
        TempPath("saga_package_manifest_escaping_ref.json"),
        R"({
  "schemaVersion": 1,
  "packageId": "starter.client",
  "packageKind": "client",
  "buildProfile": "dev-client",
  "targetPlatform": "linux",
  "runtimeCompatibilityVersion": "0.0.8",
  "assetManifests": [
    { "id": "assets.main", "path": "../Manifests/assets.json" }
  ],
  "artifactManifests": []
})");

    const auto result = PackageManifestLoader::LoadFromFile(path);

    ASSERT_FALSE(result.Succeeded());
    ASSERT_EQ(result.errors.size(), 1u);
    EXPECT_EQ(result.errors[0].diagnosticId, InvalidPath);
}

TEST(PackageManifestLoaderTests, ReferencedManifestValidationReturnsFileMissing)
{
    const auto path = WriteTempFile(
        TempPath("saga_package_manifest_missing_refs.json"),
        ValidPackageManifestJson());

    PackageManifestLoadOptions options;
    options.validateReferencedManifestFiles = true;
    options.packageBaseDirectory = TempPath("saga_package_manifest_missing_ref_root");

    const auto result = PackageManifestLoader::LoadFromFile(path, options);

    ASSERT_FALSE(result.Succeeded());
    ASSERT_EQ(result.errors.size(), 2u);
    EXPECT_EQ(result.errors[0].diagnosticId, FileMissing);
    EXPECT_EQ(result.errors[1].diagnosticId, FileMissing);
}

TEST(PackageManifestLoaderTests, ReferencedManifestValidationSucceedsWhenFilesExist)
{
    const auto root = TempPath("saga_package_manifest_existing_ref_root");
    WriteTempFile(root / "Manifests" / "assets.json", R"({"schemaVersion":1,"assets":[]})");
    WriteTempFile(root / "Manifests" / "artifacts.json", R"({"schemaVersion":1,"artifacts":[]})");
    const auto path = WriteTempFile(
        TempPath("saga_package_manifest_existing_refs.json"),
        ValidPackageManifestJson());

    PackageManifestLoadOptions options;
    options.validateReferencedManifestFiles = true;
    options.packageBaseDirectory = root;

    const auto result = PackageManifestLoader::LoadFromFile(path, options);

    ASSERT_TRUE(result.Succeeded());
    ASSERT_EQ(result.manifest.assetManifests.size(), 1u);
    ASSERT_EQ(result.manifest.artifactManifests.size(), 1u);
}

TEST(PackageManifestLoaderTests, StartupValidatorRejectsWrongPackageKind)
{
    const auto root = TempPath("saga_package_startup_wrong_kind_root");
    WriteTempFile(root / "Manifests" / "assets.json", R"({"schemaVersion":1,"assets":[]})");
    WriteTempFile(root / "Manifests" / "artifacts.json", R"({"schemaVersion":1,"artifacts":[]})");
    const auto path = WriteTempFile(
        TempPath("saga_package_startup_wrong_kind.json"),
        ValidPackageManifestJson());

    PackageStartupValidationOptions options;
    options.expectedPackageKind = PackageKind::Server;
    options.packageBaseDirectory = root;

    const auto result =
        PackageStartupValidator::ValidateManifestForStartup(path, options);

    ASSERT_FALSE(result.Succeeded());
    ASSERT_EQ(result.errors.size(), 1u);
    EXPECT_EQ(result.errors[0].diagnosticId, WrongPackageKind);
}

TEST(PackageManifestLoaderTests, StartupValidatorRejectsRuntimeMismatch)
{
    const auto root = TempPath("saga_package_startup_runtime_root");
    WriteTempFile(root / "Manifests" / "assets.json", R"({"schemaVersion":1,"assets":[]})");
    WriteTempFile(root / "Manifests" / "artifacts.json", R"({"schemaVersion":1,"artifacts":[]})");
    const auto path = WriteTempFile(
        TempPath("saga_package_startup_runtime.json"),
        ValidPackageManifestJson());

    PackageStartupValidationOptions options;
    options.expectedPackageKind = PackageKind::Client;
    options.runtimeCompatibilityVersion = "0.0.9";
    options.packageBaseDirectory = root;

    const auto result =
        PackageStartupValidator::ValidateManifestForStartup(path, options);

    ASSERT_FALSE(result.Succeeded());
    ASSERT_EQ(result.errors.size(), 1u);
    EXPECT_EQ(result.errors[0].diagnosticId, IncompatibleRuntime);
}
