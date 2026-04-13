/// @file TestSnapshotServer.cpp
/// @brief Minimal snapshot sender server implementation.

#include "TestSnapshotServer.h"

#include <SagaEngine/ECS/ComponentRegistry.h>
#include <SagaEngine/Client/Replication/ReplicationStateMachine.h>
#include <SagaEngine/Client/Replication/WorldSnapshotWire.h>
#include <SagaEngine/Input/Commands/InputCommand.h>
#include <SagaServer/Networking/Core/Packet.h>
#include <SagaServer/Networking/Core/NetworkTypes.h>

#include <cstring>
#include <chrono>

namespace Saga {

using namespace SagaEngine::Networking;
using namespace SagaEngine::Simulation;

// ─── Test component (for snapshot serialization) ─────────────────────────────

struct alignas(8) TestTransformComponent
{
    float x = 0.0f, y = 0.0f, z = 0.0f;
    float rotX = 0.0f, rotY = 0.0f, rotZ = 0.0f;
};

struct TestIdentityComponent
{
    uint32_t typeId = 0;
    char     name[32] = {};
};

static constexpr uint32_t kTestTransformTypeId = 1001;
static constexpr uint32_t kTestIdentityTypeId  = 1002;

// ─── Init ─────────────────────────────────────────────────────────────────────

bool TestSnapshotServer::Init(const TestServerConfig& config) noexcept
{
    m_config = config;

    // Register test components for ECS AddComponent<T> to work.
    auto& cr = SagaEngine::ECS::ComponentRegistry::Instance();
    if (!cr.IsRegistered<TestTransformComponent>())
    {
        SAGA_REGISTER_COMPONENT(TestTransformComponent, kTestTransformTypeId);
        SAGA_REGISTER_COMPONENT(TestIdentityComponent, kTestIdentityTypeId);
    }

    try
    {
        m_ioContext.restart();

        asio::ip::udp::endpoint listenEndpoint(
            asio::ip::address::from_string(m_config.bindHost),
            m_config.bindPort);

        m_socket = std::make_unique<asio::ip::udp::socket>(m_ioContext);
        m_socket->open(listenEndpoint.protocol());
        m_socket->set_option(asio::ip::udp::socket::reuse_address(true));
        m_socket->bind(listenEndpoint);

        LOG_INFO("TestServer", "Listening on %s:%u",
                 m_config.bindHost.c_str(), m_config.bindPort);
    }
    catch (const std::exception& e)
    {
        LOG_ERROR("TestServer", "Failed to bind: %s", e.what());
        return false;
    }

    // Create test entities in the world.
    for (uint32_t i = 0; i < m_config.testEntityCount; ++i)
    {
        auto handle = m_world.CreateEntity();
        SagaEngine::ECS::EntityId id = handle.id;

        // Add transform — entities spread out in a line.
        TestTransformComponent tc;
        tc.x = static_cast<float>(i) * 3.0f;
        tc.y = 0.0f;
        tc.z = 0.0f;
        m_world.AddComponent<TestTransformComponent>(id, tc);

        // Add identity.
        TestIdentityComponent ic;
        ic.typeId = kTestIdentityTypeId;
        std::snprintf(ic.name, sizeof(ic.name), "Entity_%u", i);
        m_world.AddComponent<TestIdentityComponent>(id, ic);

        LOG_INFO("TestServer", "Spawned test entity %u (id=%u) at x=%.1f",
                 i, id, tc.x);
    }

    return true;
}

void TestSnapshotServer::Start() noexcept
{
    m_running = true;

    m_ioThread = std::thread([this]() {
        LOG_INFO("TestServer", "IO thread started.");

        asio::ip::udp::endpoint remoteEndpoint;
        std::vector<uint8_t> recvBuf(2048);

        while (m_running)
        {
            try
            {
                // Wait for a packet (with timeout so we can check m_running).
                m_socket->async_receive_from(
                    asio::buffer(recvBuf),
                    remoteEndpoint,
                    [this, &recvBuf, &remoteEndpoint](
                        const asio::error_code& ec,
                        std::size_t bytes) {
                        if (ec || !m_running)
                            return;

                        // First packet from a client = connect.
                        if (!m_clientConnected)
                        {
                            m_clientEndpoint = remoteEndpoint;
                            m_clientConnected = true;
                            LOG_INFO("TestServer", "Client connected from %s:%u",
                                     remoteEndpoint.address().to_string().c_str(),
                                     remoteEndpoint.port());
                        }

                        // Route to handler.
                        HandleInputPacket(recvBuf.data(), bytes);
                    });

                m_ioContext.run_one_for(std::chrono::milliseconds(16));
                RunTick();
            }
            catch (const std::exception& e)
            {
                LOG_WARN("TestServer", "IO error: %s", e.what());
            }
        }

        LOG_INFO("TestServer", "IO thread stopped.");
    });

    LOG_INFO("TestServer", "Test server started.");
}

void TestSnapshotServer::Stop() noexcept
{
    m_running = false;

    if (m_ioThread.joinable())
        m_ioThread.join();

    LOG_INFO("TestServer", "Test server stopped. Snapshots sent: %llu, inputs received: %llu",
             static_cast<unsigned long long>(m_snapshotCount),
             static_cast<unsigned long long>(m_inputCount));
}

// ─── Tick ─────────────────────────────────────────────────────────────────────

void TestSnapshotServer::RunTick() noexcept
{
    if (!m_clientConnected)
        return;

    static auto lastSnapshotTime = std::chrono::steady_clock::now();
    const auto now = std::chrono::steady_clock::now();
    const float elapsed = std::chrono::duration<float>(now - lastSnapshotTime).count();

    if (elapsed >= m_config.snapshotIntervalSec)
    {
        SendSnapshotToClient();
        lastSnapshotTime = now;
    }
}

void TestSnapshotServer::SendSnapshotToClient() noexcept
{
    // Build the snapshot in the wire format the client expects.
    // Wire format (kSnapshotWireVersion=3):
    //   [Extended Header: 88 bytes]
    //     magic(4) | version(4) | serverTick(8) | captureTime(8) |
    //     zoneId(4) | shardId(4) | entityCount(4) | payloadCRC32(4) |
    //     byteLength(8) | schemaVersion(4) | protocolVersion(4) | reserved(12)
    //   [Per Entity:]
    //     entityId(4) | componentCount(2)
    //     [Per Component:]
    //       typeId(2) | dataLen(2) | data[N]

    using namespace SagaEngine::Client::Replication;

    std::vector<uint8_t> payload;
    payload.reserve(256);

    // Serialize entity components into payload.
    const auto entities = m_world.GetAliveEntities();
    uint32_t entityCount = m_world.EntityCount();

    for (EntityId id : entities)
    {
        uint32_t compCount = 0;

        // Count and serialize TestTransformComponent.
        auto* tc = m_world.GetComponent<TestTransformComponent>(id);
        if (tc) compCount++;

        auto* ic = m_world.GetComponent<TestIdentityComponent>(id);
        if (ic) compCount++;

        // Entity header.
        payload.resize(payload.size() + 6); // entityId(4) + compCount(2)
        size_t off = payload.size() - 6;
        std::memcpy(payload.data() + off, &id, 4);
        std::memcpy(payload.data() + off + 4, &compCount, 2);

        // Serialize TestTransformComponent.
        if (tc)
        {
            uint16_t typeId = kTestTransformTypeId;
            uint16_t dataLen = sizeof(TestTransformComponent);
            payload.insert(payload.end(),
                           reinterpret_cast<const uint8_t*>(&typeId),
                           reinterpret_cast<const uint8_t*>(&typeId) + 2);
            payload.insert(payload.end(),
                           reinterpret_cast<const uint8_t*>(&dataLen),
                           reinterpret_cast<const uint8_t*>(&dataLen) + 2);
            payload.insert(payload.end(),
                           reinterpret_cast<const uint8_t*>(tc),
                           reinterpret_cast<const uint8_t*>(tc) + sizeof(TestTransformComponent));
        }

        // Serialize TestIdentityComponent.
        if (ic)
        {
            uint16_t typeId = kTestIdentityTypeId;
            uint16_t dataLen = sizeof(TestIdentityComponent);
            payload.insert(payload.end(),
                           reinterpret_cast<const uint8_t*>(&typeId),
                           reinterpret_cast<const uint8_t*>(&typeId) + 2);
            payload.insert(payload.end(),
                           reinterpret_cast<const uint8_t*>(&dataLen),
                           reinterpret_cast<const uint8_t*>(&dataLen) + 2);
            payload.insert(payload.end(),
                           reinterpret_cast<const uint8_t*>(ic),
                           reinterpret_cast<const uint8_t*>(ic) + sizeof(TestIdentityComponent));
        }
    }

    // Compute CRC32 of payload (IEEE 802.3 polynomial, matching client's ComputeCRC32).
    uint32_t crc32 = 0xFFFFFFFFu;
    for (auto b : payload)
    {
        crc32 ^= static_cast<uint32_t>(b);
        for (int bit = 0; bit < 8; ++bit)
            crc32 = (crc32 >> 1) ^ ((crc32 & 1) ? 0xEDB88320u : 0);
    }
    crc32 ^= 0xFFFFFFFFu;

    // Build extended header.
    uint64_t serverTick = m_snapshotCount * 60;
    uint64_t captureTimeUs = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count());
    uint32_t zoneId = 1;
    uint32_t shardId = 1;
    uint64_t byteLength = static_cast<uint64_t>(payload.size());
    uint32_t schemaVersion = 1;
    uint32_t protocolVersion = 1;

    std::vector<uint8_t> header(88, 0);
    size_t hOff = 0;
    uint32_t magic = kSnapshotMagic;
    uint32_t version = kSnapshotWireVersion;

    std::memcpy(header.data() + hOff, &magic, 4); hOff += 4;
    std::memcpy(header.data() + hOff, &version, 4); hOff += 4;
    std::memcpy(header.data() + hOff, &serverTick, 8); hOff += 8;
    std::memcpy(header.data() + hOff, &captureTimeUs, 8); hOff += 8;
    std::memcpy(header.data() + hOff, &zoneId, 4); hOff += 4;
    std::memcpy(header.data() + hOff, &shardId, 4); hOff += 4;
    std::memcpy(header.data() + hOff, &entityCount, 4); hOff += 4;
    std::memcpy(header.data() + hOff, &crc32, 4); hOff += 4;
    std::memcpy(header.data() + hOff, &byteLength, 8); hOff += 8;
    std::memcpy(header.data() + hOff, &schemaVersion, 4); hOff += 4;
    std::memcpy(header.data() + hOff, &protocolVersion, 4); hOff += 4;
    // reserved(12) already zero.

    // Combine header + payload.
    std::vector<uint8_t> snapshotData;
    snapshotData.reserve(header.size() + payload.size());
    snapshotData.insert(snapshotData.end(), header.begin(), header.end());
    snapshotData.insert(snapshotData.end(), payload.begin(), payload.end());

    // Wrap in a Packet with Snapshot type.
    Packet pkt(PacketType::Snapshot);
    pkt.WriteBytes(snapshotData.data(), snapshotData.size());
    pkt.SetSequence(static_cast<uint32_t>(m_snapshotCount + 1));
    pkt.SetTimestamp(static_cast<uint32_t>(serverTick));

    auto data = pkt.GetSerializedData();

    m_socket->async_send_to(
        asio::buffer(data),
        m_clientEndpoint,
        [this, data](const asio::error_code& ec, std::size_t /*bytes*/) {
            if (!ec)
            {
                m_snapshotCount++;
                LOG_INFO("TestServer", "Snapshot #%llu sent (%zu bytes, %u entities)",
                         static_cast<unsigned long long>(m_snapshotCount),
                         data.size(), m_world.EntityCount());
            }
            else
            {
                LOG_WARN("TestServer", "Failed to send snapshot: %s", ec.message().c_str());
            }
        });
}

void TestSnapshotServer::HandleInputPacket(const uint8_t* data, std::size_t size) noexcept
{
    if (size < sizeof(PacketHeader))
        return;

    Packet pkt;
    if (!Packet::Deserialize(data, size, pkt))
    {
        LOG_WARN("TestServer", "Received invalid packet (%zu bytes).", size);
        return;
    }

    switch (pkt.GetType())
    {
        case PacketType::HandshakeRequest:
        {
            LOG_INFO("TestServer", "Handshake request received.");
            // Send handshake response.
            Packet resp(PacketType::HandshakeResponse);
            uint16_t protocolVersion = 1;
            uint16_t schemaVersion = 1;
            resp.Write(protocolVersion);
            resp.Write(schemaVersion);
            auto d = resp.GetSerializedData();
            m_socket->send_to(asio::buffer(d), m_clientEndpoint);
            break;
        }

        case PacketType::InputCommand:
        {
            m_inputCount++;

            // Read the InputCommand from the payload.
            const auto& serialized = pkt.GetSerializedData();
            constexpr std::size_t kHdrSize = PACKET_HEADER_SIZE;
            if (serialized.size() >= kHdrSize + sizeof(SagaEngine::Input::InputCommand))
            {
                SagaEngine::Input::InputCommand cmd;
                std::memcpy(&cmd, serialized.data() + kHdrSize, sizeof(cmd));

                LOG_INFO("TestServer", "Input #%llu: seq=%u tick=%u buttons=0x%x move=(%d,%d)",
                         static_cast<unsigned long long>(m_inputCount),
                         cmd.sequence, cmd.simulationTick, cmd.buttons,
                         cmd.moveX, cmd.moveY);
            }

            // Send InputAck back.
            m_inputSeq++;
            Packet ack(PacketType::InputAck);
            ack.Write(m_inputSeq);
            ack.SetSequence(static_cast<uint32_t>(m_inputSeq + 1000));
            auto d = ack.GetSerializedData();
            m_socket->send_to(asio::buffer(d), m_clientEndpoint);
            break;
        }

        default:
            LOG_INFO("TestServer", "Received packet type %u (%zu bytes).",
                     static_cast<unsigned>(pkt.GetType()), size);
            break;
    }
}

} // namespace Saga
