// SPDX-License-Identifier: MPL-2.0

#include "SagaEngine/Persistence/Backends/PostgreSQL/PostgreSQLImpl.h"
#include "SagaEngine/Persistence/Backends/Redis/RedisImpl.h"
#include "SagaEngine/Persistence/EventSourcing/EventLog.h"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <memory>
#include <optional>
#include <thread>
#include <utility>
#include <vector>

namespace SagaEngine::Persistence {
namespace {

class RecordingDatabase final : public IDatabase
{
public:
    bool Initialize() override { healthy = true; return true; }
    void Shutdown() override { healthy = false; }
    void WriteEntity(const EntitySnapshot&, DatabaseCallback callback) override
    {
        if (callback) callback(DatabaseStatus::Success());
    }
    void ReadEntity(EntityId, EntityReadCallback callback) override
    {
        if (callback) callback(
            DatabaseStatus::Failure(DatabaseError::NotFound, "missing"), std::nullopt);
    }
    void DeleteEntity(EntityId, DatabaseCallback callback) override
    {
        if (callback) callback(DatabaseStatus::Success());
    }
    void AppendEvent(const std::string& type, const std::vector<uint8_t>& data,
                     DatabaseCallback callback) override
    {
        lastType = type;
        lastPayload = data;
        appendCount.fetch_add(1, std::memory_order_release);
        if (callback) callback(DatabaseStatus::Success());
    }
    DatabaseStats GetStatistics() const override { return {}; }
    bool IsHealthy() const override { return healthy; }

    bool healthy = true;
    std::atomic<uint32_t> appendCount{0};
    std::string lastType;
    std::vector<uint8_t> lastPayload;
};

TEST(PersistenceContractTests, BackendsFailClosedBeforeInitialization)
{
    PersistenceConfig postgresConfig;
    postgresConfig.connectionString = "unused";
    PostgreSQLDatabase postgres(postgresConfig);
    RedisDatabase redis(RedisConfig{});
    const EntitySnapshot snapshot{.entityId = 9};

    DatabaseStatus postgresWrite;
    postgres.WriteEntity(snapshot, [&](DatabaseStatus status) {
        postgresWrite = std::move(status);
    });
    EXPECT_EQ(postgresWrite.code, DatabaseError::NotInitialized);

    DatabaseStatus postgresRead;
    postgres.ReadEntity(9, [&](DatabaseStatus status,
                               std::optional<EntitySnapshot> value) {
        postgresRead = std::move(status);
        EXPECT_FALSE(value.has_value());
    });
    EXPECT_EQ(postgresRead.code, DatabaseError::NotInitialized);

    DatabaseStatus redisWrite;
    redis.WriteEntity(snapshot, [&](DatabaseStatus status) {
        redisWrite = std::move(status);
    });
    EXPECT_EQ(redisWrite.code, DatabaseError::NotInitialized);

    DatabaseStatus redisRead;
    redis.ReadEntity(9, [&](DatabaseStatus status,
                            std::optional<EntitySnapshot> value) {
        redisRead = std::move(status);
        EXPECT_FALSE(value.has_value());
    });
    EXPECT_EQ(redisRead.code, DatabaseError::NotInitialized);

    DatabaseStatus redisDelete;
    redis.DeleteEntity(9, [&](DatabaseStatus status) {
        redisDelete = std::move(status);
    });
    EXPECT_EQ(redisDelete.code, DatabaseError::NotInitialized);
}

TEST(PersistenceContractTests, EventLogCommitsAndDoesNotClaimReplaySupport)
{
    auto database = std::make_shared<RecordingDatabase>();
    EventLogConfig config;
    config.batchSize = 4;
    config.flushIntervalMs = 1;
    EventLog log(database, config);
    ASSERT_TRUE(log.IsHealthy());

    EXPECT_EQ(log.Append("EntityChanged", {1, 2, 3}, 42), 1u);
    for (int attempt = 0;
         attempt < 100 && database->appendCount.load(std::memory_order_acquire) == 0;
         ++attempt) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    ASSERT_EQ(database->appendCount.load(std::memory_order_acquire), 1u);
    EXPECT_EQ(database->lastType, "BATCH_COMMIT");
    EXPECT_FALSE(database->lastPayload.empty());

    bool replayCallbackCalled = false;
    EXPECT_FALSE(log.ReplayFrom(1, [&](const EventRecord&) {
        replayCallbackCalled = true;
    }));
    EXPECT_FALSE(replayCallbackCalled);
}

} // namespace
} // namespace SagaEngine::Persistence
