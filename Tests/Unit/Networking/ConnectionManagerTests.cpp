/// @file ConnectionManagerTests.cpp
/// @brief Unit tests for ConnectionManager lifecycle and query API.
///
/// Layer  : SagaServer / Networking / Client
/// Purpose: Tests the ConnectionManager's lifecycle (init/shutdown), initial
///          state, query API (HasClient, IsConnected, GetConnectedCount),
///          and that the manager is safe to read concurrently.
///
/// Note: The production ConnectionManager manages real ASIO sockets via
///       AcceptSocket(). Tests here cover the observable state of a freshly
///       initialised manager (zero clients, clean stats) and concurrent reads.

#include "SagaServer/Networking/Client/ConnectionManager.h"
#include "SagaEngine/Core/Log/Log.h"

#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <atomic>

using namespace SagaServer::Networking;

class ConnectionManagerTest : public ::testing::Test
{
protected:
    asio::io_context              ioCtx;
    std::unique_ptr<ConnectionManager> _manager;

    void SetUp() override
    {
        ConnectionManagerConfig cfg;
        cfg.maxClients = 1024;
        _manager = std::make_unique<ConnectionManager>(cfg, ioCtx);
        ASSERT_TRUE(_manager->Initialize());
    }

    void TearDown() override
    {
        _manager->Shutdown();
    }
};

// ─── Basic Lifecycle ────────────────────────────────────────────────────────

TEST_F(ConnectionManagerTest, InitializeAndShutdown)
{
    ASSERT_NE(_manager, nullptr);
    EXPECT_EQ(_manager->GetConnectedCount(), 0u);
}

// ─── Initial State ──────────────────────────────────────────────────────────

TEST_F(ConnectionManagerTest, HasClientReturnsFalseForUnknownId)
{
    EXPECT_FALSE(_manager->HasClient(static_cast<ClientId>(1)));
    EXPECT_FALSE(_manager->HasClient(static_cast<ClientId>(999999)));
}

TEST_F(ConnectionManagerTest, IsConnectedReturnsFalseForUnknownId)
{
    EXPECT_FALSE(_manager->IsConnected(static_cast<ClientId>(1)));
    EXPECT_FALSE(_manager->IsConnected(static_cast<ClientId>(42)));
}

TEST_F(ConnectionManagerTest, InitialStatsAreZero)
{
    const auto& stats = _manager->GetStats();
    EXPECT_EQ(stats.totalAccepted.load(),    0u);
    EXPECT_EQ(stats.currentConnected.load(), 0u);
    EXPECT_EQ(stats.totalDisconnected.load(),0u);
}

// ─── Query Robustness ────────────────────────────────────────────────────────

TEST_F(ConnectionManagerTest, GetClientStatsReturnsFalseForUnknownId)
{
    ServerConnectionStats out{};
    EXPECT_FALSE(_manager->GetClientStats(static_cast<ClientId>(1), out));
}

TEST_F(ConnectionManagerTest, DisconnectAllOnEmptyManagerDoesNotCrash)
{
    EXPECT_NO_FATAL_FAILURE(_manager->DisconnectAll());
    EXPECT_EQ(_manager->GetConnectedCount(), 0u);
}

TEST_F(ConnectionManagerTest, DisconnectNonExistentClientDoesNotCrash)
{
    EXPECT_NO_FATAL_FAILURE(_manager->DisconnectClient(static_cast<ClientId>(999999)));
    EXPECT_EQ(_manager->GetConnectedCount(), 0u);
}

// ─── Concurrent Reads ───────────────────────────────────────────────────────
//
// Verifies that concurrent read-only access (HasClient, IsConnected,
// GetConnectedCount) is safe when no writes are happening.

TEST_F(ConnectionManagerTest, ConcurrentReadsDoNotCrash)
{
    constexpr uint32_t kThreadCount  = 8;
    constexpr uint32_t kOpsPerThread = 200;

    std::vector<std::thread> threads;
    threads.reserve(kThreadCount);

    for (uint32_t t = 0; t < kThreadCount; ++t)
    {
        threads.emplace_back([this]()
        {
            for (uint32_t i = 0; i < kOpsPerThread; ++i)
            {
                (void)_manager->GetConnectedCount();
                (void)_manager->HasClient(static_cast<ClientId>(i));
                (void)_manager->IsConnected(static_cast<ClientId>(i));
            }
        });
    }

    for (auto& th : threads)
        th.join();

    EXPECT_EQ(_manager->GetConnectedCount(), 0u);
}

// ─── Reinitialise ────────────────────────────────────────────────────────────

TEST_F(ConnectionManagerTest, ShutdownAndReinitialise)
{
    _manager->Shutdown();

    ConnectionManagerConfig cfg;
    cfg.maxClients = 512;
    _manager = std::make_unique<ConnectionManager>(cfg, ioCtx);
    ASSERT_TRUE(_manager->Initialize());
    EXPECT_EQ(_manager->GetConnectedCount(), 0u);
}
