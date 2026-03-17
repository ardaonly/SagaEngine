// Tests/Unit/Persistence/EventLogTests.cpp
#include <Persistence/EventSourcing/EventLog.h>
#include <Persistence/Database/PostgreSQLImpl.h>
#include <gtest/gtest.h>
#include <thread>
#include <chrono>

using namespace SagaEngine::Persistence;

class EventLogTest : public ::testing::Test {
protected:
    std::shared_ptr<PostgreSQLDatabase> _db;
    std::unique_ptr<EventLog> _eventLog;
    PersistenceConfig _config;

    void SetUp() override {
        const char* connEnv = std::getenv("SAGA_POSTGRES_CONN");
        _config.connectionString = connEnv ? connEnv : 
            "host=127.0.0.1 port=5432 dbname=saga_test user=postgres password=postgres";
        _config.maxConnectionPool = 5;
        
        _db = std::make_shared<PostgreSQLDatabase>(_config);
        ASSERT_TRUE(_db->Initialize());
        
        EventLogConfig logConfig;
        logConfig.batchSize = 10;
        logConfig.flushIntervalMs = 100;
        logConfig.maxQueueSize = 1024;
        
        _eventLog = std::make_unique<EventLog>(_db, logConfig);
        ASSERT_TRUE(_eventLog->IsHealthy());
    }

    void TearDown() override {
        if (_eventLog) _eventLog.reset();
        if (_db) {
            _db->Shutdown();
            _db.reset();
        }
    }
};

TEST_F(EventLogTest, Initialize) {
    EXPECT_TRUE(_eventLog->IsHealthy());
    EXPECT_EQ(_eventLog->GetCurrentSequence(), 1);
}

TEST_F(EventLogTest, AppendSingleEvent) {
    std::vector<uint8_t> data = {0x01, 0x02, 0x03};
    uint64_t seq = _eventLog->Append("TestEvent", data, 1001);
    
    EXPECT_GT(seq, 0);
    EXPECT_EQ(_eventLog->GetCurrentSequence(), 2);
    
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    auto stats = _eventLog->GetStatistics();
    EXPECT_EQ(stats.totalEvents, 1);
}

TEST_F(EventLogTest, AppendMultipleEvents) {
    for (uint32_t i = 0; i < 50; ++i) {
        std::vector<uint8_t> data = {static_cast<uint8_t>(i)};
        _eventLog->Append("BatchEvent", data, 1000 + i);
    }
    
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    auto stats = _eventLog->GetStatistics();
    EXPECT_EQ(stats.totalEvents, 50);
    EXPECT_LT(stats.failedCommits, 5);
}

TEST_F(EventLogTest, QueueBackpressure) {
    EventLogConfig strictConfig;
    strictConfig.batchSize = 5;
    strictConfig.flushIntervalMs = 1000;
    strictConfig.maxQueueSize = 10;
    
    auto strictLog = std::make_unique<EventLog>(_db, strictConfig);
    
    for (uint32_t i = 0; i < 100; ++i) {
        strictLog->Append("FloodEvent", {}, i);
    }
    
    auto stats = strictLog->GetStatistics();
    EXPECT_LT(stats.pendingEvents, strictConfig.maxQueueSize);
}

TEST_F(EventLogTest, Flush) {
    for (uint32_t i = 0; i < 5; ++i) {
        _eventLog->Append("FlushEvent", {}, i);
    }
    
    _eventLog->Flush();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    auto stats = _eventLog->GetStatistics();
    EXPECT_EQ(stats.pendingEvents, 0);
}

TEST_F(EventLogTest, ConcurrentAppend) {
    std::vector<std::thread> threads;
    std::atomic<uint64_t> successCount{0};
    
    for (int t = 0; t < 4; ++t) {
        threads.emplace_back([&, t]() {
            for (uint32_t i = 0; i < 25; ++i) {
                uint64_t seq = _eventLog->Append("ConcurrentEvent", {}, t * 100 + i);
                if (seq > 0) successCount.fetch_add(1);
            }
        });
    }
    
    for (auto& t : threads) t.join();
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    EXPECT_EQ(successCount.load(), 100);
    auto stats = _eventLog->GetStatistics();
    EXPECT_EQ(stats.totalEvents, 100);
}

TEST_F(EventLogTest, Statistics) {
    for (uint32_t i = 0; i < 10; ++i) {
        _eventLog->Append("StatEvent", {}, i);
    }
    
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    auto stats = _eventLog->GetStatistics();
    
    EXPECT_EQ(stats.totalEvents, 10);
    EXPECT_GE(stats.avgCommitLatencyMs, 0.0f);
}