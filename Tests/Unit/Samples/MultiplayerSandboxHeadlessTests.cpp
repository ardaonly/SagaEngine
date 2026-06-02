/// @file MultiplayerSandboxHeadlessTests.cpp
/// @brief Focused tests for the server-only MultiplayerSandbox headless proof.

#include "MultiplayerSandboxHeadless/HeadlessPreview.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

#ifndef SAGA_SOURCE_ROOT
#define SAGA_SOURCE_ROOT "."
#endif

namespace
{

namespace fs = std::filesystem;
using MultiplayerSandboxHeadless::HeadlessPreviewOptions;
using MultiplayerSandboxHeadless::RunHeadlessPreview;
using MultiplayerSandboxHeadless::WriteHeadlessPreviewReportJson;

[[nodiscard]] fs::path SourceRoot()
{
    return fs::path(SAGA_SOURCE_ROOT);
}

[[nodiscard]] fs::path SampleProject()
{
    return SourceRoot() / "samples" / "MultiplayerSandbox" /
        "MultiplayerSandbox.sagaproj";
}

[[nodiscard]] fs::path TempRoot(const char* name)
{
    fs::path path = fs::temp_directory_path() / name;
    fs::remove_all(path);
    fs::create_directories(path);
    return path;
}

} // namespace

TEST(MultiplayerSandboxHeadlessTests, MovementQueuesBeforeTickAndMutatesDuringTick)
{
    const fs::path root = TempRoot("MultiplayerSandboxHeadless_Movement");
    HeadlessPreviewOptions options;
    options.projectPath = SampleProject();
    options.reportOut = root / "headless_server_report.json";
    options.diagnosticsOut = root / "Diagnostics";
    options.ticks = 1;
    options.fixedDtSeconds = 1.0f;

    const auto report = RunHeadlessPreview(options);

    EXPECT_EQ(report.status, "Passed");
    EXPECT_EQ(report.projectId, "multiplayer-sandbox");
    EXPECT_EQ(report.entityCount, 1);
    EXPECT_EQ(report.inputQueuedCount, 1);
    EXPECT_EQ(report.inputAcceptedCount, 1);
    ASSERT_EQ(report.dirtyEntityIds.size(), 1u);
    EXPECT_EQ(report.dirtyEntityIds.front(), 1001u);
    EXPECT_FLOAT_EQ(report.beforeTickPosition.x, report.initialPosition.x);
    EXPECT_FLOAT_EQ(report.beforeTickPosition.y, report.initialPosition.y);
    EXPECT_FLOAT_EQ(report.beforeTickPosition.z, report.initialPosition.z);
    EXPECT_GT(report.finalPosition.y, report.initialPosition.y);
    EXPECT_TRUE(report.diagnostics.empty());
}

TEST(MultiplayerSandboxHeadlessTests, WritesDeterministicReport)
{
    const fs::path root = TempRoot("MultiplayerSandboxHeadless_Report");
    HeadlessPreviewOptions options;
    options.projectPath = SampleProject();
    options.reportOut = root / "headless_server_report.json";
    options.diagnosticsOut = root / "Diagnostics";

    const auto report = RunHeadlessPreview(options);
    std::string error;

    ASSERT_TRUE(WriteHeadlessPreviewReportJson(report, options.reportOut, error))
        << error;
    ASSERT_TRUE(fs::exists(options.reportOut));

    std::ifstream input(options.reportOut);
    std::string text(
        (std::istreambuf_iterator<char>(input)),
        std::istreambuf_iterator<char>());
    EXPECT_NE(text.find("\"tool\": \"MultiplayerSandboxHeadless\""), std::string::npos);
    EXPECT_NE(text.find("\"status\": \"Passed\""), std::string::npos);
    EXPECT_NE(text.find("\"projectId\": \"multiplayer-sandbox\""), std::string::npos);
}

TEST(MultiplayerSandboxHeadlessTests, DoesNotMutateProjectManifest)
{
    const fs::path root = TempRoot("MultiplayerSandboxHeadless_ReadOnly");
    const fs::path manifest = SampleProject();
    std::ifstream beforeInput(manifest);
    const std::string before(
        (std::istreambuf_iterator<char>(beforeInput)),
        std::istreambuf_iterator<char>());

    HeadlessPreviewOptions options;
    options.projectPath = manifest;
    options.reportOut = root / "headless_server_report.json";
    options.diagnosticsOut = root / "Diagnostics";

    const auto report = RunHeadlessPreview(options);
    ASSERT_EQ(report.status, "Passed");

    std::ifstream afterInput(manifest);
    const std::string after(
        (std::istreambuf_iterator<char>(afterInput)),
        std::istreambuf_iterator<char>());
    EXPECT_EQ(after, before);
}

TEST(MultiplayerSandboxHeadlessTests, MissingManifestFailsDeterministically)
{
    const fs::path root = TempRoot("MultiplayerSandboxHeadless_Missing");
    HeadlessPreviewOptions options;
    options.projectPath = root / "Missing.sagaproj";
    options.reportOut = root / "headless_server_report.json";
    options.diagnosticsOut = root / "Diagnostics";

    const auto report = RunHeadlessPreview(options);

    EXPECT_EQ(report.status, "Failed");
    ASSERT_FALSE(report.diagnostics.empty());
    EXPECT_EQ(
        report.diagnostics.front().code,
        "MultiplayerSandbox.Project.ManifestMissing");
}
