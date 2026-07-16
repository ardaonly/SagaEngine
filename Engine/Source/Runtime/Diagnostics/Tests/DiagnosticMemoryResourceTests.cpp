/// @file DiagnosticMemoryResourceTests.cpp
/// @brief Verifies diagnostics memory and resource snapshot foundations.

#include <SagaEngine/Diagnostics/DiagnosticSystem.hpp>
#include <SagaEngine/Diagnostics/Memory/MemoryScope.hpp>
#include <SagaEngine/Diagnostics/Report/DiagnosticReportWriter.hpp>
#include <SagaEngine/Diagnostics/Resource/ResourceTracker.hpp>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <limits>
#include <sstream>
#include <string>
#include <vector>

namespace
{

using SagaEngine::Diagnostics::CrashReport;
using SagaEngine::Diagnostics::DiagnosticReport;
using SagaEngine::Diagnostics::DiagnosticReportWriter;
using SagaEngine::Diagnostics::DiagnosticSystem;
using SagaEngine::Diagnostics::MemoryScope;
using SagaEngine::Diagnostics::MemoryTracker;
using SagaEngine::Diagnostics::ResourceTracker;
using SagaEngine::Diagnostics::ResourceType;
namespace CrashReportKinds = SagaEngine::Diagnostics::CrashReportKinds;

nlohmann::json WriteToJson(const DiagnosticReport& report)
{
    std::ostringstream output;
    const auto result = DiagnosticReportWriter::WriteJson(report, output);
    EXPECT_TRUE(result.Succeeded());
    return nlohmann::json::parse(output.str());
}

nlohmann::json WriteToJson(const CrashReport& report)
{
    std::ostringstream output;
    const auto result = DiagnosticReportWriter::WriteJson(report, output);
    EXPECT_TRUE(result.Succeeded());
    return nlohmann::json::parse(output.str());
}

std::vector<std::string> MemorySystemNames(const nlohmann::json& report)
{
    std::vector<std::string> names;
    for (const auto& record : report.at("memoryRecords"))
    {
        names.push_back(record.at("systemName").get<std::string>());
    }
    return names;
}

} // namespace

TEST(DiagnosticMemoryResourceTests, MemoryTrackerDefaultSnapshotIsEmpty)
{
    MemoryTracker tracker;

    const auto snapshot = tracker.Snapshot();

    EXPECT_TRUE(snapshot.Records().empty());
    EXPECT_EQ(snapshot.Summary().systemCount, 0u);
    EXPECT_EQ(snapshot.Summary().currentBytes, 0u);
}

TEST(DiagnosticMemoryResourceTests, MemoryTrackerTracksBytesAndPeaks)
{
    MemoryTracker tracker;

    EXPECT_TRUE(tracker.AddBytes("World", 100));
    EXPECT_TRUE(tracker.RecordAllocation("World", 50));
    EXPECT_TRUE(tracker.RemoveBytes("World", 25));

    const auto snapshot = tracker.Snapshot();
    ASSERT_EQ(snapshot.Records().size(), 1u);
    const auto& record = snapshot.Records()[0];
    EXPECT_EQ(record.systemName, "World");
    EXPECT_EQ(record.currentBytes, 125u);
    EXPECT_EQ(record.peakBytes, 150u);
    EXPECT_EQ(record.totalAllocatedBytes, 150u);
    EXPECT_EQ(record.totalFreedBytes, 25u);
    EXPECT_EQ(record.activeAllocationCount, 1u);
}

TEST(DiagnosticMemoryResourceTests, MemoryTrackerRejectsOverRemovalDeterministically)
{
    MemoryTracker tracker;
    ASSERT_TRUE(tracker.AddBytes("Movement", 64));

    EXPECT_FALSE(tracker.RemoveBytes("Movement", 65));

    const auto snapshot = tracker.Snapshot();
    ASSERT_EQ(snapshot.Records().size(), 1u);
    EXPECT_EQ(snapshot.Records()[0].currentBytes, 64u);
    EXPECT_EQ(snapshot.Records()[0].totalFreedBytes, 0u);
}

TEST(DiagnosticMemoryResourceTests, MemoryTrackerRejectsOverflowDeterministically)
{
    MemoryTracker tracker;
    constexpr auto max = std::numeric_limits<std::uint64_t>::max();
    ASSERT_TRUE(tracker.RecordAllocation("Overflow", max));

    EXPECT_FALSE(tracker.AddBytes("Overflow", 1));

    const auto snapshot = tracker.Snapshot();
    ASSERT_EQ(snapshot.Records().size(), 1u);
    EXPECT_EQ(snapshot.Records()[0].currentBytes, max);
    EXPECT_EQ(snapshot.Records()[0].totalAllocatedBytes, max);
}

TEST(DiagnosticMemoryResourceTests, MemorySnapshotOrderingIsDeterministic)
{
    MemoryTracker tracker;
    ASSERT_TRUE(tracker.AddBytes("ZoneServer", 10));
    ASSERT_TRUE(tracker.AddBytes("Movement", 20));
    ASSERT_TRUE(tracker.AddBytes("StressArena", 30));

    const auto snapshot = tracker.Snapshot();

    ASSERT_EQ(snapshot.Records().size(), 3u);
    EXPECT_EQ(snapshot.Records()[0].systemName, "Movement");
    EXPECT_EQ(snapshot.Records()[1].systemName, "StressArena");
    EXPECT_EQ(snapshot.Records()[2].systemName, "ZoneServer");
}

TEST(DiagnosticMemoryResourceTests, MemoryScopeRecordsAndReleases)
{
    MemoryTracker tracker;
    {
        MemoryScope scope(tracker, "Scoped", 128);
        const auto snapshot = tracker.Snapshot();
        ASSERT_EQ(snapshot.Records().size(), 1u);
        EXPECT_EQ(snapshot.Records()[0].currentBytes, 128u);
        EXPECT_EQ(snapshot.Records()[0].activeAllocationCount, 1u);
    }

    const auto snapshot = tracker.Snapshot();
    ASSERT_EQ(snapshot.Records().size(), 1u);
    EXPECT_EQ(snapshot.Records()[0].currentBytes, 0u);
    EXPECT_EQ(snapshot.Records()[0].activeAllocationCount, 0u);
}

TEST(DiagnosticMemoryResourceTests, ResourceTrackerRegistersAndReleases)
{
    ResourceTracker tracker;

    const auto id = tracker.RegisterResource(
        ResourceType::Timer,
        "StressArena",
        "duration-cap",
        7);
    ASSERT_NE(id, 0u);
    EXPECT_TRUE(tracker.ReleaseResource(id, 8));

    const auto all = tracker.SnapshotAll();
    ASSERT_EQ(all.Records().size(), 1u);
    EXPECT_TRUE(all.Records()[0].released);
    EXPECT_EQ(all.Records()[0].releasedTick, 8u);
    EXPECT_TRUE(tracker.SnapshotActive().Records().empty());
}

TEST(DiagnosticMemoryResourceTests, ResourceTrackerInvalidAndDoubleReleaseAreDeterministic)
{
    ResourceTracker tracker;

    EXPECT_FALSE(tracker.ReleaseResource(999, 1));
    const auto id = tracker.RegisterResource(
        ResourceType::Job,
        "StressArena",
        "runner-job",
        2);
    ASSERT_TRUE(tracker.ReleaseResource(id, 3));
    EXPECT_FALSE(tracker.ReleaseResource(id, 4));

    const auto all = tracker.SnapshotAll();
    ASSERT_EQ(all.Records().size(), 1u);
    EXPECT_EQ(all.Records()[0].releasedTick, 3u);
}

TEST(DiagnosticMemoryResourceTests, ResourceLeakSummaryUsesActiveRecordsOnly)
{
    ResourceTracker tracker;

    const auto released = tracker.RegisterResource(
        ResourceType::File,
        "AssetPipeline",
        "manifest",
        1);
    ASSERT_TRUE(tracker.ReleaseResource(released, 2));
    (void)tracker.RegisterResource(
        ResourceType::AssetHandle,
        "Runtime",
        "texture",
        3);
    (void)tracker.RegisterResource(
        ResourceType::AssetHandle,
        "Runtime",
        "mesh",
        4);
    (void)tracker.RegisterResource(
        ResourceType::Timer,
        "Server",
        "tick",
        5);

    const auto summary = tracker.BuildResourceLeakSummary();

    EXPECT_EQ(summary.totalActive, 3u);
    EXPECT_EQ(summary.byType.at("asset_handle"), 2u);
    EXPECT_EQ(summary.byType.at("timer"), 1u);
    EXPECT_EQ(summary.byOwnerSystem.at("Runtime"), 2u);
    EXPECT_EQ(summary.byOwnerSystem.at("Server"), 1u);
    EXPECT_EQ(summary.byType.count("file"), 0u);
}

TEST(DiagnosticMemoryResourceTests, DiagnosticSystemExposesMemoryAndResourceTrackers)
{
    DiagnosticSystem diagnostics;

    EXPECT_TRUE(diagnostics.Memory().AddBytes("World", 42));
    const auto id = diagnostics.Resource().RegisterResource(
        ResourceType::Other,
        "World",
        "explicit-resource",
        1);
    EXPECT_NE(id, 0u);

    EXPECT_EQ(diagnostics.Memory().Snapshot().Summary().currentBytes, 42u);
    EXPECT_EQ(diagnostics.Resource().SnapshotActive().Records().size(), 1u);
}

TEST(DiagnosticMemoryResourceTests, OperationalReportIncludesMemoryAndResources)
{
    DiagnosticSystem diagnostics;
    ASSERT_TRUE(diagnostics.Memory().RecordAllocation("StressArena", 512));
    (void)diagnostics.Resource().RegisterResource(
        ResourceType::Other,
        "StressArena",
        "direct-harness",
        1);

    const auto report = diagnostics.BuildOperationalSnapshot();

    EXPECT_EQ(report.summary.memorySystemCount, 1u);
    EXPECT_EQ(report.summary.activeResourceCount, 1u);
    ASSERT_EQ(report.memoryRecords.size(), 1u);
    EXPECT_EQ(report.memoryRecords[0].systemName, "StressArena");
    EXPECT_EQ(report.resourceSummary.totalActive, 1u);

    const auto json = WriteToJson(report);
    EXPECT_EQ(json["summary"]["memorySystemCount"], 1u);
    EXPECT_EQ(json["memoryRecords"][0]["currentBytes"], 512u);
    EXPECT_EQ(json["resourceSummary"]["byOwnerSystem"]["StressArena"], 1u);
    EXPECT_EQ(json["activeResources"][0]["resourceType"], "other");
}

TEST(DiagnosticMemoryResourceTests, ReliabilityReportIncludesMemoryAndResources)
{
    DiagnosticSystem diagnostics;
    ASSERT_TRUE(diagnostics.Memory().RecordAllocation("ZoneServer", 1024));
    (void)diagnostics.Resource().RegisterResource(
        ResourceType::Timer,
        "ZoneServer",
        "tick-budget",
        10);

    const auto report = diagnostics.BuildReliabilityReport(
        CrashReportKinds::ReliabilityFailureReport,
        "memory resource capture");

    EXPECT_EQ(report.summary.memorySystemCount, 1u);
    EXPECT_EQ(report.summary.activeResourceCount, 1u);
    ASSERT_EQ(report.memoryRecords.size(), 1u);
    EXPECT_EQ(report.memoryRecords[0].systemName, "ZoneServer");
    EXPECT_EQ(report.resourceSummary.byType.at("timer"), 1u);

    const auto json = WriteToJson(report);
    EXPECT_EQ(json["summary"]["memorySystemCount"], 1u);
    EXPECT_EQ(json["memoryRecords"][0]["systemName"], "ZoneServer");
    EXPECT_EQ(json["resourceSummary"]["totalActive"], 1u);
    EXPECT_EQ(json["activeResources"][0]["debugName"], "tick-budget");
}
