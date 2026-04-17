/// @file TestSnapshotServer.h
/// @brief Minimal snapshot sender server — validates client roundtrip.
///
/// Layer  : Apps / Server
/// Purpose: Before wiring WorldNode into the full SagaServer, we need a
///          minimal server that proves the client can:
///            1. Connect via UDP
///            2. Receive a WorldState snapshot
///            3. Deserialize it into the client's ECS
///            4. Display entities via debug render
///
///          This server:
///            - Listens on UDP port
///            - On client connect, spawns N test entities in a WorldState
///            - Sends serialized WorldState as Snapshot packets
///            - Receives InputCommand packets, applies them to entities,
///              and echoes the command sequence back as InputAck
///            - Sends InputAck packets with the echoed command sequence

#pragma once

#include <SagaServer/Networking/Core/NetworkTransport.h>
#include <SagaServer/Networking/Core/ReliableChannel.h>
#include <SagaServer/Networking/Core/Packet.h>
#include <SagaEngine/Simulation/WorldState.h>
#include <SagaEngine/Core/Log/Log.h>
#include <SagaEngine/Core/Time/Time.h>
#include <SagaEngine/Input/Commands/InputCommand.h>

#include <asio.hpp>
#include <cstdint>
#include <memory>
#include <string>
#include <thread>
#include <atomic>
#include <mutex>
#include <vector>

namespace Saga {

struct TestServerConfig
{
    std::string bindHost = "127.0.0.1";
    uint16_t    bindPort = 7777;
    uint32_t    testEntityCount = 5;
    float       snapshotIntervalSec = 0.5f;  // 2 Hz snapshots
};

class TestSnapshotServer
{
public:
    TestSnapshotServer() = default;
    ~TestSnapshotServer() = default;

    TestSnapshotServer(const TestSnapshotServer&)            = delete;
    TestSnapshotServer& operator=(const TestSnapshotServer&) = delete;

    bool Init(const TestServerConfig& config) noexcept;
    void Start() noexcept;
    void Stop() noexcept;

private:
    void RunTick() noexcept;
    void SendSnapshotToClient() noexcept;
    void HandleInputPacket(const uint8_t* data, std::size_t size) noexcept;

    // ─── Members ─────────────────────────────────────────────────────────────

    TestServerConfig m_config{};

    asio::io_context    m_ioContext;
    std::unique_ptr<asio::ip::udp::socket> m_socket;
    std::thread         m_ioThread;
    std::atomic<bool>   m_running{false};

    asio::ip::udp::endpoint m_clientEndpoint;
    bool                    m_clientConnected = false;

    SagaEngine::Simulation::WorldState m_world;
    uint64_t                           m_snapshotCount = 0;
    uint64_t                           m_inputCount    = 0;
};

} // namespace Saga
