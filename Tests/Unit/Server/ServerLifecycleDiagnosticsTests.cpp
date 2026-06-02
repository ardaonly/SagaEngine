/// @file ServerLifecycleDiagnosticsTests.cpp
/// @brief Verifies bounded direct/local ZoneServer lifecycle diagnostics.

#include "SagaServer/Networking/Server/ZoneServer.h"

#include "SagaEngine/Diagnostics/DiagnosticConfig.hpp"
#include "SagaEngine/Diagnostics/DiagnosticSystem.hpp"
#include "SagaEngine/Diagnostics/Report/DiagnosticReportWriter.hpp"
#include "SagaEngine/Diagnostics/ServerLifecycle/ServerLifecycleTracker.hpp"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <cstdint>
#include <sstream>
#include <string>

namespace
{

using SagaEngine::Diagnostics::DiagnosticReport;
using SagaEngine::Diagnostics::DiagnosticReportWriter;
using SagaEngine::Diagnostics::DiagnosticSystem;
using SagaEngine::Diagnostics::ServerLifecycleTracker;
using SagaEngine::Networking::ClientId;
using SagaEngine::Server::Simulation::ActorOwnershipResult;
using SagaEngine::Server::Simulation::EntityId;
using SagaEngine::Server::Simulation::Vector3;
using SagaServer::Networking::ZoneServer;
using SagaServer::Networking::ZoneServerConfig;

constexpr ClientId kClient = 77;
constexpr EntityId kEntity = 1201;

ZoneServerConfig TestConfig()
{
    ZoneServerConfig config;
    config.bindAddress = "127.0.0.1";
    config.port = 0;
    config.zoneId = 33;
    config.zoneName = "server-lifecycle-diagnostics-test";
    config.maxTickBudgetUs = 1000;
    return config;
}

nlohmann::json WriteToJson(const DiagnosticReport& report)
{
    std::ostringstream output;
    const auto result = DiagnosticReportWriter::WriteJson(report, output);
    EXPECT_TRUE(result.Succeeded());
    return nlohmann::json::parse(output.str());
}

bool HasEvent(const DiagnosticSystem& diagnostics, const std::string& eventName)
{
    const auto snapshot = diagnostics.ServerLifecycle().Snapshot();
    return std::any_of(snapshot.events.begin(),
                       snapshot.events.end(),
                       [&](const auto& event) {
                           return event.eventName == eventName;
                       });
}

std::size_t CountEvents(
    const DiagnosticSystem& diagnostics,
    const std::string& eventName)
{
    const auto snapshot = diagnostics.ServerLifecycle().Snapshot();
    return static_cast<std::size_t>(
        std::count_if(snapshot.events.begin(),
                      snapshot.events.end(),
                      [&](const auto& event) {
                          return event.eventName == eventName;
                      }));
}

} // namespace

TEST(ServerLifecycleDiagnosticsTests, DefaultTrackerSnapshotIsEmptyAndDeterministic)
{
    ServerLifecycleTracker tracker;

    const auto first = tracker.Snapshot();
    const auto second = tracker.Snapshot();

    EXPECT_TRUE(first.events.empty());
    EXPECT_TRUE(first.records.empty());
    EXPECT_TRUE(first.leaks.empty());
    EXPECT_EQ(first.summary.eventCount, 0u);
    EXPECT_EQ(first.summary.recordCount, 0u);
    EXPECT_EQ(first.summary.activeRecordCount, 0u);
    EXPECT_EQ(first.summary.leakCount, 0u);
    EXPECT_EQ(first.summary.droppedEventCount, 0u);
    EXPECT_EQ(first.summary.droppedRecordCount, 0u);
    EXPECT_EQ(second.summary.eventCount, first.summary.eventCount);
    EXPECT_EQ(second.summary.droppedEventCount, first.summary.droppedEventCount);
}

TEST(ServerLifecycleDiagnosticsTests, BoundedCapsIncrementDroppedCounts)
{
    ServerLifecycleTracker tracker(1, 1);

    EXPECT_TRUE(tracker.RecordEvent("A", "server", "info", "stored"));
    EXPECT_FALSE(tracker.RecordEvent("B", "server", "info", "dropped"));
    EXPECT_EQ(tracker.CreateRecord("SessionConnection", 4, 10), 1u);
    EXPECT_EQ(tracker.CreateRecord("Entity", 4, 11), 0u);

    const auto snapshot = tracker.Snapshot();
    ASSERT_EQ(snapshot.events.size(), 1u);
    ASSERT_EQ(snapshot.records.size(), 1u);
    EXPECT_EQ(snapshot.events[0].sequence, 1u);
    EXPECT_EQ(snapshot.records[0].recordId, 1u);
    EXPECT_EQ(snapshot.summary.droppedEventCount, 1u);
    EXPECT_EQ(snapshot.summary.droppedRecordCount, 1u);
}

TEST(ServerLifecycleDiagnosticsTests, PayloadFieldsAreSorted)
{
    ServerLifecycleTracker tracker;
    ASSERT_TRUE(tracker.RecordEvent(
        "TickSlow",
        "tick",
        "warning",
        "slow",
        1,
        2,
        {{"duration_us", "2000"}, {"budget_us", "1000"}}));

    const auto snapshot = tracker.Snapshot();
    ASSERT_EQ(snapshot.events.size(), 1u);
    ASSERT_EQ(snapshot.events[0].payload.size(), 2u);
    EXPECT_EQ(snapshot.events[0].payload[0].first, "budget_us");
    EXPECT_EQ(snapshot.events[0].payload[1].first, "duration_us");
}

TEST(ServerLifecycleDiagnosticsTests, ServerStartAndShutdownEmitLifecycleEvents)
{
    DiagnosticSystem diagnostics;
    ZoneServer server(TestConfig());
    server.SetDiagnostics(&diagnostics);

    server.RecordServerStartedForTesting();
    server.RequestShutdown();

    EXPECT_TRUE(HasEvent(diagnostics, "ServerStarted"));
    EXPECT_TRUE(HasEvent(diagnostics, "ServerShutdownRequested"));
    EXPECT_TRUE(HasEvent(diagnostics, "LifetimeLeakDetected"));

    const auto snapshot = diagnostics.ServerLifecycle().Snapshot();
    ASSERT_GE(snapshot.records.size(), 1u);
    EXPECT_EQ(snapshot.records[0].recordKind, "Server");
    EXPECT_TRUE(snapshot.records[0].active);
}

TEST(ServerLifecycleDiagnosticsTests, SessionConnectDisconnectCreatesAndClosesRecord)
{
    DiagnosticSystem diagnostics;
    ZoneServer server(TestConfig());
    server.SetDiagnostics(&diagnostics);

    server.RecordSessionConnectedForTesting(kClient);
    server.RecordSessionDisconnectedForTesting(kClient);

    const auto snapshot = diagnostics.ServerLifecycle().Snapshot();
    ASSERT_EQ(snapshot.records.size(), 1u);
    EXPECT_EQ(snapshot.records[0].recordKind, "SessionConnection");
    EXPECT_EQ(snapshot.records[0].externalId, kClient);
    EXPECT_FALSE(snapshot.records[0].active);
    EXPECT_EQ(snapshot.records[0].status, "disconnected");
    EXPECT_TRUE(HasEvent(diagnostics, "SessionConnected"));
    EXPECT_TRUE(HasEvent(diagnostics, "SessionDisconnected"));
}

TEST(ServerLifecycleDiagnosticsTests, EntityCreateDestroyCreatesAndClosesRecord)
{
    DiagnosticSystem diagnostics;
    ZoneServer server(TestConfig());
    server.SetDiagnostics(&diagnostics);
    server.RecordSessionConnectedForTesting(kClient);

    ASSERT_EQ(server.RegisterControlledActor(kClient, kEntity, Vector3{}),
              ActorOwnershipResult::Registered);
    ASSERT_EQ(server.UnregisterControlledActor(kClient),
              ActorOwnershipResult::Unregistered);

    const auto snapshot = diagnostics.ServerLifecycle().Snapshot();
    const auto entityIt = std::find_if(
        snapshot.records.begin(),
        snapshot.records.end(),
        [](const auto& record) { return record.recordKind == "Entity"; });
    ASSERT_NE(entityIt, snapshot.records.end());
    EXPECT_EQ(entityIt->externalId, kEntity);
    EXPECT_FALSE(entityIt->active);
    EXPECT_EQ(entityIt->status, "destroyed");
    EXPECT_TRUE(HasEvent(diagnostics, "EntityCreated"));
    EXPECT_TRUE(HasEvent(diagnostics, "EntityDestroyed"));
}

TEST(ServerLifecycleDiagnosticsTests, TickSlowUsesExplicitDurationAndBudget)
{
    DiagnosticSystem diagnostics;
    ZoneServer server(TestConfig());
    server.SetDiagnostics(&diagnostics);

    server.RecordTickDurationForTesting(12, 1001);

    const auto snapshot = diagnostics.ServerLifecycle().Snapshot();
    const auto eventIt = std::find_if(
        snapshot.events.begin(),
        snapshot.events.end(),
        [](const auto& event) { return event.eventName == "TickSlow"; });
    ASSERT_NE(eventIt, snapshot.events.end());
    EXPECT_EQ(eventIt->tick, 12u);
    ASSERT_EQ(eventIt->payload.size(), 2u);
    EXPECT_EQ(eventIt->payload[0].first, "budget_us");
    EXPECT_EQ(eventIt->payload[0].second, "1000");
    EXPECT_EQ(eventIt->payload[1].first, "duration_us");
    EXPECT_EQ(eventIt->payload[1].second, "1001");
}

TEST(ServerLifecycleDiagnosticsTests, LifetimeLeakDetectedRequiresExplicitTransition)
{
    DiagnosticSystem diagnostics;
    ZoneServer server(TestConfig());
    server.SetDiagnostics(&diagnostics);
    server.RecordSessionConnectedForTesting(kClient);

    EXPECT_EQ(CountEvents(diagnostics, "LifetimeLeakDetected"), 0u);
    (void)diagnostics.ServerLifecycle().Snapshot();
    (void)diagnostics.BuildOperationalSnapshot();
    EXPECT_EQ(CountEvents(diagnostics, "LifetimeLeakDetected"), 0u);

    EXPECT_EQ(diagnostics.ServerLifecycle().EmitLifetimeLeakEventsForActiveRecords(
                  TestConfig().zoneId,
                  99),
              1u);
    EXPECT_EQ(CountEvents(diagnostics, "LifetimeLeakDetected"), 1u);
}

TEST(ServerLifecycleDiagnosticsTests, ReportSerializesEmptyServerLifecycleSection)
{
    DiagnosticSystem diagnostics;

    const auto report = diagnostics.BuildOperationalSnapshot();
    const auto json = WriteToJson(report);

    ASSERT_TRUE(json.contains("serverLifecycle"));
    EXPECT_TRUE(json["serverLifecycle"]["events"].empty());
    EXPECT_TRUE(json["serverLifecycle"]["records"].empty());
    EXPECT_TRUE(json["serverLifecycle"]["leaks"].empty());
    EXPECT_EQ(json["serverLifecycle"]["summary"]["eventCount"], 0u);
    EXPECT_EQ(json["serverLifecycle"]["summary"]["recordCount"], 0u);
    EXPECT_EQ(json["serverLifecycle"]["summary"]["activeRecordCount"], 0u);
    EXPECT_EQ(json["serverLifecycle"]["summary"]["leakCount"], 0u);
    EXPECT_EQ(json["serverLifecycle"]["summary"]["droppedEventCount"], 0u);
    EXPECT_EQ(json["serverLifecycle"]["summary"]["droppedRecordCount"], 0u);
}

TEST(ServerLifecycleDiagnosticsTests, DiagnosticsAbsentPreservesEntityBehavior)
{
    ZoneServer server(TestConfig());

    EXPECT_EQ(server.RegisterControlledActor(kClient, kEntity, Vector3{}),
              ActorOwnershipResult::Registered);
    EXPECT_TRUE(server.FindControlledActor(kClient).has_value());
    EXPECT_EQ(server.UnregisterControlledActor(kClient),
              ActorOwnershipResult::Unregistered);
    EXPECT_FALSE(server.FindControlledActor(kClient).has_value());
}

TEST(ServerLifecycleDiagnosticsTests, SnapshotConstructionIsIdempotentAndDoesNotMutateTracker)
{
    DiagnosticSystem diagnostics;
    ASSERT_TRUE(diagnostics.ServerLifecycle().RecordEvent(
        "SessionConnected",
        "session",
        "info",
        "connected",
        1,
        2));
    ASSERT_EQ(diagnostics.ServerLifecycle().CreateRecord(
                  "SessionConnection",
                  1,
                  42,
                  0,
                  2,
                  "connected",
                  "client:42"),
              1u);

    const auto snapshotBefore = diagnostics.ServerLifecycle().Snapshot();
    const auto reportOne = diagnostics.BuildOperationalSnapshot();
    const auto reportTwo = diagnostics.BuildOperationalSnapshot();
    const auto snapshotAfter = diagnostics.ServerLifecycle().Snapshot();

    EXPECT_EQ(snapshotAfter.summary.eventCount, snapshotBefore.summary.eventCount);
    EXPECT_EQ(snapshotAfter.summary.recordCount, snapshotBefore.summary.recordCount);
    EXPECT_EQ(snapshotAfter.summary.leakCount, snapshotBefore.summary.leakCount);
    EXPECT_EQ(snapshotAfter.summary.droppedEventCount,
              snapshotBefore.summary.droppedEventCount);
    EXPECT_EQ(snapshotAfter.summary.droppedRecordCount,
              snapshotBefore.summary.droppedRecordCount);
    ASSERT_EQ(snapshotAfter.events.size(), 1u);
    EXPECT_EQ(snapshotAfter.events[0].sequence, snapshotBefore.events[0].sequence);
    EXPECT_EQ(reportOne.serverLifecycle.summary.eventCount,
              reportTwo.serverLifecycle.summary.eventCount);
    EXPECT_EQ(CountEvents(diagnostics, "LifetimeLeakDetected"), 0u);
}
