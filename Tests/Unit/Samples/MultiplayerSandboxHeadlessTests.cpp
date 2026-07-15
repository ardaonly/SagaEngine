/// @file MultiplayerSandboxHeadlessTests.cpp
/// @brief Focused tests for the server-only MultiplayerSandbox headless proof.

#include "MultiplayerSandboxHeadless/HeadlessEvaluation.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <fstream>

#ifndef SAGA_SOURCE_ROOT
#define SAGA_SOURCE_ROOT "."
#endif

namespace
{

namespace fs = std::filesystem;
using MultiplayerSandboxHeadless::HeadlessEvaluationOptions;
using MultiplayerSandboxHeadless::RunHeadlessEvaluation;
using MultiplayerSandboxHeadless::WriteHeadlessEvaluationReportJson;

[[nodiscard]] fs::path SourceRoot()
{
    return fs::path(SAGA_SOURCE_ROOT);
}

[[nodiscard]] fs::path SampleProject()
{
    return SourceRoot() / "Samples" / "MultiplayerSandbox" /
        "MultiplayerSandbox.sagaproj";
}

[[nodiscard]] fs::path StarterArenaProject()
{
    return SourceRoot() / "Samples" / "StarterArena" /
        "StarterArena.sagaproj";
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
    HeadlessEvaluationOptions options;
    options.projectPath = SampleProject();
    options.reportOut = root / "headless_server_report.json";
    options.diagnosticsOut = root / "Diagnostics";
    options.ticks = 1;
    options.fixedDtSeconds = 1.0f;

    const auto report = RunHeadlessEvaluation(options);

    EXPECT_EQ(report.status, "Passed");
    EXPECT_EQ(report.projectId, "multiplayer-sandbox");
    EXPECT_EQ(report.entityCount, 1);
    EXPECT_EQ(report.inputQueuedCount, 1);
    EXPECT_EQ(report.inputAcceptedCount, 1);
    ASSERT_EQ(report.dirtyEntityIds.size(), 1u);
    EXPECT_EQ(report.dirtyEntityIds.front(), 1001u);
    EXPECT_TRUE(report.serverAuthority);
    EXPECT_EQ(report.networkMode, "HeadlessSmoke");
    EXPECT_EQ(report.snapshotCount, 1);
    EXPECT_FLOAT_EQ(report.beforeTickPosition.x, report.initialPosition.x);
    EXPECT_FLOAT_EQ(report.beforeTickPosition.y, report.initialPosition.y);
    EXPECT_FLOAT_EQ(report.beforeTickPosition.z, report.initialPosition.z);
    EXPECT_GT(report.finalPosition.y, report.initialPosition.y);
    EXPECT_TRUE(report.diagnostics.empty());
}

TEST(MultiplayerSandboxHeadlessTests, WritesDeterministicReport)
{
    const fs::path root = TempRoot("MultiplayerSandboxHeadless_Report");
    HeadlessEvaluationOptions options;
    options.projectPath = SampleProject();
    options.reportOut = root / "headless_server_report.json";
    options.diagnosticsOut = root / "Diagnostics";

    const auto report = RunHeadlessEvaluation(options);
    std::string error;

    ASSERT_TRUE(WriteHeadlessEvaluationReportJson(report, options.reportOut, error))
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

TEST(MultiplayerSandboxHeadlessTests, StarterArenaServerSmokeIsAuthoritative)
{
    const fs::path root = TempRoot("StarterArenaServerSmoke_Authority");
    HeadlessEvaluationOptions options;
    options.projectPath = StarterArenaProject();
    options.reportOut = root / "starter_arena_server_report.json";
    options.diagnosticsOut = root / "Diagnostics";
    options.starterArenaServerSmoke = true;
    options.ticks = 1;
    options.fixedDtSeconds = 1.0f;

    const auto report = RunHeadlessEvaluation(options);

    EXPECT_EQ(report.status, "Passed");
    EXPECT_EQ(report.projectId, "starter-arena");
    EXPECT_TRUE(report.serverAuthority);
    EXPECT_EQ(report.networkMode, "HeadlessSmoke");
    EXPECT_EQ(report.entityCount, 1);
    EXPECT_EQ(report.inputQueuedCount, 1);
    EXPECT_EQ(report.inputAcceptedCount, 1);
    EXPECT_EQ(report.inputRejectedCount, 1);
    EXPECT_EQ(report.snapshotCount, 1);
    ASSERT_EQ(report.dirtyEntityIds.size(), 1u);
    EXPECT_EQ(report.dirtyEntityIds.front(), 1001u);
    ASSERT_EQ(report.invalidInputDiagnostics.size(), 1u);
    EXPECT_EQ(
        report.invalidInputDiagnostics.front().code,
        "StarterArena.ServerSmoke.InvalidInputRejected");
    EXPECT_FLOAT_EQ(
        report.authoritativeInitialState.x,
        report.beforeTickPosition.x);
    EXPECT_GT(
        report.authoritativeFinalState.x,
        report.authoritativeInitialState.x);
    EXPECT_TRUE(report.diagnostics.empty());
    EXPECT_NE(
        std::find(
            report.nonClaims.begin(),
            report.nonClaims.end(),
            "Not full multiplayer"),
        report.nonClaims.end());
}

TEST(MultiplayerSandboxHeadlessTests, StarterArenaServerSmokeWritesEvidenceReport)
{
    const fs::path root = TempRoot("StarterArenaServerSmoke_Report");
    HeadlessEvaluationOptions options;
    options.projectPath = StarterArenaProject();
    options.reportOut = root / "starter_arena_server_report.json";
    options.diagnosticsOut = root / "Diagnostics";
    options.starterArenaServerSmoke = true;

    const auto report = RunHeadlessEvaluation(options);
    std::string error;

    ASSERT_TRUE(WriteHeadlessEvaluationReportJson(report, options.reportOut, error))
        << error;
    ASSERT_TRUE(fs::exists(options.reportOut));

    std::ifstream input(options.reportOut);
    std::string text(
        (std::istreambuf_iterator<char>(input)),
        std::istreambuf_iterator<char>());
    EXPECT_NE(text.find("\"projectId\": \"starter-arena\""), std::string::npos);
    EXPECT_NE(text.find("\"serverAuthority\": true"), std::string::npos);
    EXPECT_NE(text.find("\"networkMode\": \"HeadlessSmoke\""), std::string::npos);
    EXPECT_NE(text.find("\"inputAcceptedCount\": 1"), std::string::npos);
    EXPECT_NE(text.find("\"inputRejectedCount\": 1"), std::string::npos);
    EXPECT_NE(text.find("\"snapshotCount\": 1"), std::string::npos);
    EXPECT_NE(text.find("\"authoritativeInitialState\""), std::string::npos);
    EXPECT_NE(text.find("\"authoritativeFinalState\""), std::string::npos);
    EXPECT_NE(text.find("\"invalidInputDiagnostics\""), std::string::npos);
    EXPECT_NE(text.find("\"Not full multiplayer\""), std::string::npos);
}

TEST(MultiplayerSandboxHeadlessTests, DoesNotMutateProjectManifest)
{
    const fs::path root = TempRoot("MultiplayerSandboxHeadless_ReadOnly");
    const fs::path manifest = SampleProject();
    std::ifstream beforeInput(manifest);
    const std::string before(
        (std::istreambuf_iterator<char>(beforeInput)),
        std::istreambuf_iterator<char>());

    HeadlessEvaluationOptions options;
    options.projectPath = manifest;
    options.reportOut = root / "headless_server_report.json";
    options.diagnosticsOut = root / "Diagnostics";

    const auto report = RunHeadlessEvaluation(options);
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
    HeadlessEvaluationOptions options;
    options.projectPath = root / "Missing.sagaproj";
    options.reportOut = root / "headless_server_report.json";
    options.diagnosticsOut = root / "Diagnostics";

    const auto report = RunHeadlessEvaluation(options);

    EXPECT_EQ(report.status, "Failed");
    ASSERT_FALSE(report.diagnostics.empty());
    EXPECT_EQ(
        report.diagnostics.front().code,
        "MultiplayerSandbox.Project.ManifestMissing");
}
