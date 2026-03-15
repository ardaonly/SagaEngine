// Tests/Integration/Persistence/PostgreSQLTests.cpp
#include <Persistence/Database/PostgreSQLImpl.h>
#include <Persistence/Types.h>
#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <future>
#include <mutex>
#include <string>
#include <vector>

using namespace SagaEngine::Persistence;
using namespace std::chrono_literals;

class PostgreSQLTest : public ::testing::Test {
protected:
    std::unique_ptr<PostgreSQLDatabase> _db;
    PersistenceConfig _config;

    static constexpr auto kShortTimeout = 5s;
    static constexpr auto kMediumTimeout = 10s;
    static constexpr auto kLongTimeout = 30s;

    void SetUp() override {
        const char* connEnv = std::getenv("SAGA_POSTGRES_CONN");

        if (connEnv) {
            _config.connectionString = connEnv;
        } else {
            _config.connectionString = "host=127.0.0.1 port=5432 dbname=saga_test user=postgres password=postgres";
        }

        _config.maxConnectionPool = 5;
        _config.writeQueueSize = 1024;

        _db = std::make_unique<PostgreSQLDatabase>(_config);
        ASSERT_TRUE(_db->Initialize()) << "Failed to initialize PostgreSQLDatabase";
        ASSERT_TRUE(_db->IsHealthy()) << "PostgreSQLDatabase is not healthy after initialization";
        
        _db->ClearTestData(1000, 10000);
    }

    void TearDown() override {
        if (_db) {
            _db->Shutdown();
            _db.reset();
        }
    }

    template<typename Pred>
    bool WaitForPredicate(Pred pred, std::condition_variable& cv, std::mutex& m, std::chrono::milliseconds timeout) {
        auto deadline = std::chrono::steady_clock::now() + timeout;
        std::unique_lock lock(m);
        while (!pred()) {
            if (cv.wait_until(lock, deadline) == std::cv_status::timeout) break;
        }
        return pred();
    }
};

TEST_F(PostgreSQLTest, Initialize) {
    EXPECT_TRUE(_db->IsHealthy());
}

TEST_F(PostgreSQLTest, WriteAndReadEntity) {
    EntitySnapshot snapshot;
    snapshot.entityId = 1001;
    snapshot.version = 1;
    snapshot.timestamp = 0;
    snapshot.data = {0x01, 0x02};
    snapshot.componentTypes = {1};

    std::promise<bool> writePromise;
    auto writeFuture = writePromise.get_future();

    _db->WriteEntity(snapshot, [&](bool ok, const std::string& /*msg*/) {
        writePromise.set_value(ok);
    });

    auto status = writeFuture.wait_for(kMediumTimeout);
    ASSERT_EQ(status, std::future_status::ready) << "Timed out waiting for WriteEntity callback";
    EXPECT_TRUE(writeFuture.get()) << "WriteEntity reported failure";

    std::promise<bool> readPromise;
    auto readFuture = readPromise.get_future();

    _db->ReadEntity(snapshot.entityId, [&](bool ok, const std::string& /*msg*/) {
        readPromise.set_value(ok);
    });

    status = readFuture.wait_for(kMediumTimeout);
    ASSERT_EQ(status, std::future_status::ready) << "Timed out waiting for ReadEntity callback";
    EXPECT_TRUE(readFuture.get()) << "ReadEntity reported failure";
}

TEST_F(PostgreSQLTest, Statistics) {
    auto stats = _db->GetStatistics();
    EXPECT_GE(stats.totalWrites, 0u);
    EXPECT_GE(stats.pendingWrites, 0u);
}

TEST_F(PostgreSQLTest, QueuePressure) {
    constexpr uint32_t kWrites = 100;
    std::atomic<uint32_t> writeCount{0};
    std::atomic<uint32_t> errorCount{0};

    std::mutex m;
    std::condition_variable cv;

    for (uint32_t i = 0; i < kWrites; ++i) {
        EntitySnapshot snapshot;
        snapshot.entityId = 2000 + i;
        snapshot.version = 1;
        snapshot.data = {static_cast<uint8_t>(i & 0xFF)};
        snapshot.componentTypes = {1};

        _db->WriteEntity(snapshot, [&](bool ok, const std::string& /*msg*/) {
            writeCount.fetch_add(1, std::memory_order_relaxed);
            if (!ok) errorCount.fetch_add(1, std::memory_order_relaxed);
            if ((writeCount.load(std::memory_order_relaxed) % 16) == 0 || 
                writeCount.load(std::memory_order_relaxed) == kWrites) {
                std::lock_guard lk(m);
                cv.notify_one();
            }
        });
    }

    bool reached = false;
    {
        auto pred = [&](){ return writeCount.load(std::memory_order_relaxed) >= kWrites; };
        reached = WaitForPredicate(pred, cv, m, 
            std::chrono::duration_cast<std::chrono::milliseconds>(kLongTimeout));
    }

    ASSERT_TRUE(reached) << "Timed out before all write callbacks were received (got " 
                         << writeCount.load() << ")";
    EXPECT_EQ(writeCount.load(), kWrites);
    EXPECT_LT(errorCount.load(), 5u);
}