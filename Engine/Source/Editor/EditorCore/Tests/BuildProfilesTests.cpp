// SPDX-License-Identifier: MPL-2.0

#include "SagaEditor/Performance/BuildProfiles.h"

#include <gtest/gtest.h>

#include <chrono>
#include <fstream>

namespace SagaEditor::Tests
{
namespace fs = std::filesystem;

class BuildProfilesTests : public testing::Test
{
protected:
    void SetUp() override
    {
        root = fs::temp_directory_path() / ("saga-build-profiles-" +
            std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        fs::create_directories(root);
    }
    void TearDown() override { fs::remove_all(root); }
    fs::path Write(std::string_view bytes)
    {
        const auto path = root / "BuildProfiles.saga.json";
        std::ofstream(path) << bytes;
        return path;
    }
    fs::path root;
};

TEST_F(BuildProfilesTests, LoadsAndConvertsValidatedBenchmarkWithoutProjectMutation)
{
    const auto path = Write(R"({"schemaVersion":1,"profiles":[
      {"id":"diagnostics","kind":"Diagnostics","requestedCounters":["lifecycle-transitions"]},
      {"id":"arena","kind":"Benchmark","scenarioId":"arena-rush.collectibles",
       "warmup":2,"trials":5,"measuredIterations":100,"fixedStepSeconds":0.0166666667,
       "seed":42,"workload":5,"requestedCounters":["update-time"],
       "runtimeMode":"ManagedRuntime","evidencePath":"Build/Reports/Benchmarks/arena.json"}]})");
    const auto before = fs::last_write_time(path);
    const auto loaded = BuildProfiles::Load(path, root);
    ASSERT_TRUE(loaded.succeeded);
    const auto [input, diagnostics] = BuildProfiles::CreateRunInput(loaded.document, "arena");
    EXPECT_TRUE(diagnostics.empty());
    EXPECT_EQ(input.workload, 5);
    EXPECT_FALSE(input.profileHash.empty());
    EXPECT_EQ(fs::last_write_time(path), before);
    EXPECT_FALSE(fs::exists(root / "ArenaRush.sagaproj"));
    EXPECT_FALSE(fs::exists(root / "SagaPackage.lock"));
}

TEST_F(BuildProfilesTests, RejectsUnknownVersionAmbiguousScenarioAndEscapedEvidence)
{
    auto unsupported = BuildProfiles::Load(Write(R"({"schemaVersion":2,"profiles":[]})"), root);
    EXPECT_FALSE(unsupported.succeeded);
    auto escaped = BuildProfiles::Load(Write(R"({"schemaVersion":1,"profiles":[
      {"id":"a","kind":"Benchmark","scenarioId":"same","warmup":0,"trials":1,
       "measuredIterations":1,"fixedStepSeconds":0.1,"seed":1,"workload":1,
       "requestedCounters":[],"runtimeMode":"ManagedRuntime","evidencePath":"../escape"},
      {"id":"b","kind":"Benchmark","scenarioId":"same","warmup":0,"trials":1,
       "measuredIterations":1,"fixedStepSeconds":0.1,"seed":1,"workload":1,
       "requestedCounters":[],"runtimeMode":"ManagedRuntime","evidencePath":"Build/Reports/Benchmarks/b"}]})"), root);
    EXPECT_FALSE(escaped.succeeded);
    EXPECT_GE(escaped.diagnostics.size(), 2U);
}

TEST(BuildProfilesStatisticsTests, ReportsMedianTailPercentilesAndPopulationVariance)
{
    const auto statistics = BuildProfiles::CalculateStatistics({1, 2, 3, 4, 100});
    EXPECT_DOUBLE_EQ(statistics.medianNanoseconds, 3);
    EXPECT_DOUBLE_EQ(statistics.p95Nanoseconds, 100);
    EXPECT_DOUBLE_EQ(statistics.p99Nanoseconds, 100);
    EXPECT_DOUBLE_EQ(statistics.varianceNanosecondsSquared, 1522.0);
}
}
