// SPDX-License-Identifier: MPL-2.0

#include "SagaEngine/World/EventStream.h"
#include "SagaEngine/World/WorldNode.h"

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <string>
#include <vector>

namespace SagaEngine::World {
namespace {

struct TemporaryPaths
{
    TemporaryPaths()
    {
        const auto unique = std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count());
        log = std::filesystem::temp_directory_path() / ("saga-events-" + unique + ".log");
        snapshot = std::filesystem::temp_directory_path() / ("saga-snapshot-" + unique + ".bin");
    }

    ~TemporaryPaths()
    {
        std::error_code error;
        std::filesystem::remove(log, error);
        std::filesystem::remove(snapshot, error);
    }

    std::filesystem::path log;
    std::filesystem::path snapshot;
};

TEST(EventStreamTests, UsesConfiguredPathsAndRoundTripsEventsAndSnapshot)
{
    TemporaryPaths paths;
    const auto logPath = paths.log.string();
    const auto snapshotPath = paths.snapshot.string();
    EventStreamConfig config;
    config.logPath = logPath.c_str();
    config.snapshotPath = snapshotPath.c_str();
    config.flushBytes = 1;
    config.fsyncOnFlush = false;

    {
        EventStream writer(config);
        ASSERT_TRUE(writer.Open());
        WorldEvent event;
        event.type = WorldEventType::EntitySpawned;
        event.entityId = 17;
        event.payload = {4, 5, 6};
        writer.Append(std::move(event));
        writer.Close();

        WorldSnapshot snapshot;
        snapshot.baseSequence = 1;
        snapshot.worldTick = 90;
        snapshot.timestampUs = 1234;
        snapshot.stateData = {9, 8, 7, 6};
        ASSERT_TRUE(writer.SaveSnapshot(snapshot));
    }

    EventStream reader(config);
    ASSERT_TRUE(reader.Open());
    ASSERT_TRUE(reader.LoadFromFile());
    ASSERT_EQ(reader.EventCount(), 1u);
    ASSERT_NE(reader.GetEvent(1), nullptr);
    EXPECT_EQ(reader.GetEvent(1)->entityId, 17u);
    EXPECT_EQ(reader.GetEvent(1)->payload, (std::vector<uint8_t>{4, 5, 6}));

    const auto restored = reader.LoadLatestSnapshot();
    ASSERT_TRUE(restored.has_value());
    EXPECT_EQ(restored->baseSequence, 1u);
    EXPECT_EQ(restored->worldTick, 90u);
    EXPECT_EQ(restored->timestampUs, 1234u);
    EXPECT_EQ(restored->stateData, (std::vector<uint8_t>{9, 8, 7, 6}));
}

TEST(WorldNodeReplicationBoundaryTests, SuppressesSnapshotsWithoutRealComponentData)
{
    WorldNode node;
    ASSERT_TRUE(node.Init(WorldNodeConfig{}));
    const SimCellId cell{0, 0};
    node.SpawnEntity(1, cell);
    node.SpawnEntity(2, cell);
    node.MarkEntityDirty(2, ComponentMask{1});
    node.Relevance().SetEdge(1, RelevanceEdge{
        .target = 2,
        .weight = 1.0f,
        .reason = RelevanceReason::Custom,
    });
    const auto sessionId = node.AddClientSession(1);

    EXPECT_FALSE(node.ReplicateToClient(sessionId));
    ASSERT_NE(node.GetClientSession(sessionId), nullptr);
    EXPECT_TRUE(node.GetClientSession(sessionId)->needsFullSnapshot);
    EXPECT_EQ(node.Stats().snapshotsSent, 0u);

    node.Shutdown();
}

} // namespace
} // namespace SagaEngine::World
