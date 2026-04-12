/// @file ClientHost.cpp
/// @brief MMO client application host implementation.
///
/// ClientNetworkSession is defined inline here to avoid stale-header cache

#include "ClientHost.h"
#include <SagaEngine/Core/Log/Log.h>
#include <SagaEngine/Core/Time/Time.h>
#include <SagaEngine/Input/Commands/InputCommand.h>
#include <SagaEngine/Math/Transform.h>

#include <SagaEngine/Client/Interpolation/InterpolationBuffer.h>
#include <SagaEngine/Client/Prediction/ReconciliationBuffer.h>
#include <SagaEngine/Client/Replication/PacketDemux.h>
#include <SagaEngine/Client/Replication/ReplicationApplyBridge.h>
#include <SagaEngine/Client/Replication/ReplicationStateMachine.h>
#include <SagaEngine/Client/Replication/SnapshotApplyPipeline.h>
#include <SagaEngine/Client/Replication/WorldSnapshotWire.h>
#include <SagaEngine/Input/Commands/InputCommandBuffer.h>
#include <SagaEngine/Simulation/WorldState.h>

#include <SagaServer/Networking/Core/NetworkTransport.h>
#include <SagaServer/Networking/Core/ReliableChannel.h>
#include <SagaServer/Networking/Core/Packet.h>

#include <SDL2/SDL.h>

#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace Saga {

// ─── Namespace aliases (cpp-only, no header pollution) ────────────────────

namespace Net       = SagaEngine::Networking;
namespace ClientRep = SagaEngine::Client::Replication;
namespace ClientPred = SagaEngine::Client::Prediction;
namespace ClientInt = SagaEngine::Client;  // InterpolationManager lives here
namespace Sim       = SagaEngine::Simulation;
namespace Input     = SagaEngine::Input;
namespace ECS       = SagaEngine::ECS;

static constexpr const char* kTag = "ClientHost";

// ─── Helpers ──────────────────────────────────────────────────────────────

namespace {

uint32_t CurrentTimeMs() noexcept
{
    using namespace std::chrono;
    return static_cast<uint32_t>(
        duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count());
}

} // anonymous namespace

// ============================================================================
// ClientNetworkSession — defined entirely in this TU to avoid stale headers
// ============================================================================

// ─── Forward declarations ─────────────────────────────────────────────────

struct ClientSessionConfig
{
    std::string serverHost = "127.0.0.1";
    uint16_t    serverPort = 7777;
    ECS::EntityId localPlayerId = 0;
    float fixedDtSeconds       = 1.0f / 60.0f;
    float positionErrorThresholdSq = 0.04f;
    double serverTickHz        = 20.0;
    float interpolationRenderDelay = 0.1f;
};

struct ReceiveRing
{
    static constexpr std::uint32_t kCapacity = 256;
    static constexpr std::uint32_t kMaxPacketBytes = 1400;

    struct Slot
    {
        std::uint8_t data[kMaxPacketBytes]{};
        std::uint32_t size = 0;
        std::uint64_t recvTick = 0;
    };

    std::array<Slot, kCapacity> slots{};
    std::atomic<std::uint32_t>  writePos{0};
    std::atomic<std::uint32_t>  readPos{0};

    bool Push(const uint8_t* data, std::uint32_t size, std::uint64_t tick) noexcept
    {
        const auto pos = writePos.load(std::memory_order_relaxed);
        const auto next = (pos + 1) % kCapacity;
        if (next == readPos.load(std::memory_order_acquire))
            return false;
        auto& s = slots[pos];
        s.size = size < kMaxPacketBytes ? size : kMaxPacketBytes;
        if (size > 0) std::memcpy(s.data, data, s.size);
        s.recvTick = tick;
        writePos.store(next, std::memory_order_release);
        return true;
    }

    bool Pop(const uint8_t*& outData, std::uint32_t& outSize, std::uint64_t& outTick) noexcept
    {
        const auto pos = readPos.load(std::memory_order_relaxed);
        if (pos == writePos.load(std::memory_order_acquire))
            return false;
        const auto& s = slots[pos];
        outData = s.data;
        outSize = s.size;
        outTick = s.recvTick;
        readPos.store((pos + 1) % kCapacity, std::memory_order_release);
        return true;
    }
};

struct ClientSessionStats
{
    std::uint64_t framesTicked          = 0;
    std::uint64_t packetsReceived       = 0;
    std::uint64_t packetsSent           = 0;
    std::uint64_t inputCommandsSent     = 0;
    std::uint64_t inputAcksReceived     = 0;
    std::uint64_t fullSnapshotsApplied  = 0;
    std::uint64_t deltaSnapshotsApplied = 0;
    std::uint64_t reconcileRewinds      = 0;
    float         currentRttMs          = 0.0f;
    ClientRep::ReplicationState state   = ClientRep::ReplicationState::Boot;
};

// ─── Session class ────────────────────────────────────────────────────────

class ClientNetworkSession
{
public:
    ClientNetworkSession() = default;
    ~ClientNetworkSession() = default;

    bool Configure(const ClientSessionConfig& config = {});
    bool Connect();
    void Disconnect();

    void TickReceive();
    void Tick(std::uint64_t frameTick);
    void TickSend();

    bool QueueInput(const Input::InputCommand& cmd);
    std::vector<Input::InputCommand> PeekPendingInput() const;
    void MarkInputSent(std::uint32_t count);

    ClientRep::ReplicationState GetReplicationState() const
        { return m_stateMachine.CurrentState(); }
    ClientSessionStats GetStats() const { return m_stats; }

    void SetLocalPlayerId(ECS::EntityId id);
    ECS::EntityId GetLocalPlayerId() const { return m_config.localPlayerId; }

    ClientInt::InterpolationManager& GetInterpolationManager()
        { return m_interpolation; }
    const ClientInt::InterpolationManager& GetInterpolationManager() const
        { return m_interpolation; }
    Sim::WorldState& GetWorldState() { return m_world; }
    Input::InputCommandBuffer& GetInputBuffer() { return m_inputBuffer; }

    using OnStateChangedFn = std::function<void(Net::ConnectionState)>;
    void SetOnStateChanged(OnStateChangedFn fn) { m_onStateChanged = std::move(fn); }

private:
    bool SetupBridge();
    bool DecodeFullSnapshot(const std::vector<std::uint8_t>& payload,
                            ClientRep::DecodedSnapshotFrame& outFrame);
    bool DecodeDeltaSnapshot(const std::vector<std::uint8_t>& payload,
                             ClientRep::DecodedSnapshotFrame& outFrame);

    void HandleInputAck(const Net::Packet& pkt);
    bool SendReliable(Net::PacketType type, const uint8_t* payload, std::size_t size);

    // ─── Members ──────────────────────────────────────────────────────────

    ClientSessionConfig m_config{};

    std::unique_ptr<Net::INetworkTransport> m_transport;
    std::unique_ptr<Net::ReliableChannel>   m_reliableChannel;

    ClientRep::ReplicationStateMachine  m_stateMachine;
    ClientRep::PacketDemux              m_packetDemux;
    ClientRep::SnapshotApplyPipeline    m_pipeline;
    ClientRep::ReplicationApplyBridge   m_bridge;
    ClientRep::ReplicationApplyBridgeConfig m_bridgeConfig{};

    Sim::WorldState                     m_world;
    ClientPred::ReconciliationBuffer<128> m_reconciliation;
    ClientInt::InterpolationManager     m_interpolation;
    Input::InputCommandBuffer           m_inputBuffer;

    ReceiveRing                         m_receiveRing;
    std::uint32_t                       m_inputSequence = 0;
    std::uint64_t                       m_frameCounter  = 0;

    ClientSessionStats                  m_stats{};

    OnStateChangedFn                    m_onStateChanged;

    bool m_localPlayerSet = false;
};

// ─── Session implementation ───────────────────────────────────────────────

bool ClientNetworkSession::Configure(const ClientSessionConfig& config)
{
    m_config = config;

    // ── 1. Transport ────────────────────────────────────────────────────────
    m_transport = Net::TransportFactory::Create(true /* UDP */);
    if (!m_transport)
    {
        LOG_ERROR(kTag, "Failed to create UDP transport.");
        return false;
    }

    Net::NetworkConfig netConfig;
    netConfig.connectTimeoutMs    = 5000;
    netConfig.keepAliveIntervalMs = 2000;
    netConfig.maxPacketSize       = 1400;

    if (!m_transport->Initialize(netConfig))
    {
        LOG_ERROR(kTag, "Failed to initialize UDP transport.");
        return false;
    }

    // ── 2. Reliable channel ─────────────────────────────────────────────────
    m_reliableChannel = std::make_unique<Net::ReliableChannel>(/* connectionId */ 1);
    Net::ReliableChannelConfig rcConfig;
    rcConfig.windowSize         = 64;
    rcConfig.maxRetransmissions = 5;
    rcConfig.rttTimeoutMs       = 100;
    m_reliableChannel->Initialize(rcConfig);
    m_reliableChannel->SetConnected(false);

    m_reliableChannel->SetOnRttUpdated([this](float rttMs) {
        m_stateMachine.UpdateRtt(rttMs);
        m_stats.currentRttMs = rttMs;
    });

    // ── 3. State machine ────────────────────────────────────────────────────
    ClientRep::StateMachineConfig smConfig;
    smConfig.hashCheckIntervalTicks = 300;
    smConfig.desyncGraceTicks       = 2;
    m_stateMachine.Configure(smConfig);

    // ── 4. Packet demux ─────────────────────────────────────────────────────
    ClientRep::PacketDemuxConfig demuxConfig;
    demuxConfig.clientProtocolVersion = 1;
    demuxConfig.clientSchemaVersion   = 1;
    m_packetDemux.Configure(demuxConfig);
    m_packetDemux.SetStateMachine(&m_stateMachine);

    m_packetDemux.SetCallbacks(
        [this](ClientRep::DecodedWorldSnapshot&& snap) {
            auto outcome = m_pipeline.SubmitFull(std::move(snap));
            if (outcome == ClientRep::ApplyOutcome::Ok)
            {
                m_stats.fullSnapshotsApplied++;
            }
        },
        [this](ClientRep::DecodedDeltaSnapshot&& delta) {
            auto outcome = m_pipeline.SubmitDelta(std::move(delta));
            if (outcome == ClientRep::ApplyOutcome::Ok)
            {
                m_stats.deltaSnapshotsApplied++;
            }
        },
        [this](const char* reason) {
            LOG_WARN(kTag, "Demux bounds violation: %s", reason);
        });

    // ── 5. Interpolation ────────────────────────────────────────────────────
    ClientInt::InterpolationConfig interpConfig;
    interpConfig.renderDelaySeconds = config.interpolationRenderDelay;
    m_interpolation.SetConfig(interpConfig);

    // ── 6. Bridge ───────────────────────────────────────────────────────────
    if (!SetupBridge())
    {
        LOG_ERROR(kTag, "Failed to set up ReplicationApplyBridge.");
        return false;
    }

    LOG_INFO(kTag, "Client session configured. Server: %s:%u",
             config.serverHost.c_str(), config.serverPort);
    return true;
}

bool ClientNetworkSession::Connect()
{
    Net::NetworkAddress addr(m_config.serverHost, m_config.serverPort);
    if (!m_transport->Connect(addr))
    {
        LOG_ERROR(kTag, "Failed to connect to %s:%u",
                  m_config.serverHost.c_str(), m_config.serverPort);
        return false;
    }

    m_transport->SetOnConnected([this](Net::ConnectionId /*cid*/) {
        LOG_INFO(kTag, "UDP connected.");
        m_reliableChannel->SetConnected(true);
        m_stateMachine.TransitionTo(ClientRep::ReplicationState::AwaitingFullSnapshot);
        if (m_onStateChanged)
            m_onStateChanged(Net::ConnectionState::Connected);
    });

    m_transport->SetOnDisconnected([this](Net::ConnectionId /*cid*/, int reason) {
        LOG_WARN(kTag, "UDP disconnected. Reason: %d", reason);
        m_reliableChannel->SetConnected(false);
        m_stateMachine.TransitionTo(ClientRep::ReplicationState::Boot);
        if (m_onStateChanged)
            m_onStateChanged(Net::ConnectionState::Disconnected);
    });

    m_transport->SetOnPacketReceived([this](Net::ConnectionId /*cid*/,
                                             const uint8_t* data,
                                             std::size_t size) {
        m_receiveRing.Push(data, static_cast<std::uint32_t>(size), m_frameCounter);
    });

    LOG_INFO(kTag, "Connecting to server %s:%u …",
             m_config.serverHost.c_str(), m_config.serverPort);
    return true;
}

void ClientNetworkSession::Disconnect()
{
    LOG_INFO(kTag, "Disconnecting from server.");

    if (m_transport)
        m_transport->Disconnect(0);

    m_reliableChannel->Shutdown();
    m_pipeline.Reset();
    m_bridge.Reset();
    m_stateMachine.Reset();
    m_packetDemux.SetStateMachine(nullptr);
    m_inputBuffer.Reset();
    m_reconciliation.Clear();
    m_interpolation.Clear();
    m_world = Sim::WorldState{};
    m_localPlayerSet = false;

    LOG_INFO(kTag, "Session disconnected.");
}

void ClientNetworkSession::TickReceive()
{
    const uint8_t* data = nullptr;
    std::uint32_t  size = 0;
    std::uint64_t  tick = 0;

    while (m_receiveRing.Pop(data, size, tick))
    {
        m_stats.packetsReceived++;
        m_reliableChannel->Receive(data, size);
    }

    auto receivedPackets = m_reliableChannel->GetReceivedPackets();
    for (const auto& pkt : receivedPackets)
    {
        if (pkt.GetType() == Net::PacketType::InputAck)
        {
            HandleInputAck(pkt);
            m_stats.inputAcksReceived++;
            continue;
        }

        auto serialized = pkt.GetSerializedData();
        constexpr std::size_t kHdrSize = Net::PACKET_HEADER_SIZE;
        if (serialized.size() <= kHdrSize)
            continue;

        const auto pktType = static_cast<std::uint16_t>(pkt.GetType());
        const auto pktSeq  = pkt.GetSequence();

        m_packetDemux.Enqueue(
            pktType, pktSeq,
            serialized.data() + kHdrSize,
            static_cast<std::uint32_t>(serialized.size() - kHdrSize),
            tick);
    }

    m_packetDemux.ProcessQueue(m_frameCounter);
}

void ClientNetworkSession::Tick(std::uint64_t frameTick)
{
    m_frameCounter = frameTick;
    m_stats.framesTicked++;

    m_stateMachine.Tick(m_frameCounter);

    if (m_pipeline.LastAppliedTick() != ClientRep::kInvalidTick)
        m_pipeline.Tick(m_pipeline.LastAppliedTick());

    m_reliableChannel->Update(CurrentTimeMs());

    auto smStats = m_stateMachine.Stats();
    if (smStats.currentRttMs > 0.0f)
        m_stats.currentRttMs = smStats.currentRttMs;
    m_stats.state = m_stateMachine.CurrentState();
}

void ClientNetworkSession::TickSend()
{
    auto pending = m_inputBuffer.PeekPending();
    for (const auto& cmd : pending)
    {
        const uint8_t* cmdBytes = reinterpret_cast<const uint8_t*>(&cmd);
        SendReliable(Net::PacketType::InputCommand, cmdBytes, sizeof(Input::InputCommand));
        m_stats.inputCommandsSent++;
        m_inputBuffer.MarkSent(1);
    }

    auto toSend = m_reliableChannel->GetPacketsToSend();
    for (const auto& pkt : toSend)
    {
        auto serialized = pkt.GetSerializedData();
        m_transport->Send(serialized.data(), serialized.size());
        m_stats.packetsSent++;
    }
}

bool ClientNetworkSession::QueueInput(const Input::InputCommand& cmd)
{
    return m_inputBuffer.PushLocal(cmd);
}

std::vector<Input::InputCommand> ClientNetworkSession::PeekPendingInput() const
{
    return m_inputBuffer.PeekPending();
}

void ClientNetworkSession::MarkInputSent(std::uint32_t count)
{
    m_inputBuffer.MarkSent(count);
}

void ClientNetworkSession::SetLocalPlayerId(ECS::EntityId id)
{
    if (m_config.localPlayerId != id)
    {
        LOG_INFO(kTag, "Local player id set to %u (was %u).",
                 id, m_config.localPlayerId);
    }
    m_config.localPlayerId = id;
    m_localPlayerSet = true;
    m_bridgeConfig.localPlayerId = id;
}

bool ClientNetworkSession::SetupBridge()
{
    m_bridgeConfig.localPlayerId            = m_config.localPlayerId;
    m_bridgeConfig.fixedDtSeconds           = m_config.fixedDtSeconds;
    m_bridgeConfig.positionErrorThresholdSq = m_config.positionErrorThresholdSq;
    m_bridgeConfig.serverTickHz             = m_config.serverTickHz;

    auto ok = m_bridge.Configure(
        m_pipeline,
        &m_world,
        &m_reconciliation,
        &m_interpolation,
        [this](const std::vector<std::uint8_t>& payload, ClientRep::DecodedSnapshotFrame& out) {
            return DecodeFullSnapshot(payload, out);
        },
        [this](const std::vector<std::uint8_t>& payload, ClientRep::DecodedSnapshotFrame& out) {
            return DecodeDeltaSnapshot(payload, out);
        },
        m_bridgeConfig);

    if (!ok)
        return false;

    m_bridge.SetReplayFn(
        [this](std::uint32_t /*inputSeq*/,
               float        dtSeconds,
               ClientPred::PredictedState& stateOut) {
            stateOut.position += stateOut.velocity * dtSeconds;
        });

    return true;
}

bool ClientNetworkSession::DecodeFullSnapshot(const std::vector<std::uint8_t>& payload,
                                                ClientRep::DecodedSnapshotFrame& outFrame)
{
    if (payload.empty())
        return false;

    // Decode using the wire format decoder.
    auto result = ClientRep::DecodeSnapshotWire(payload.data(), payload.size());
    if (!result.success)
    {
        LOG_ERROR(kTag, "Wire decode failed: %s", result.error.c_str());
        return false;
    }

    const auto& wire = result.decoded;

    LOG_INFO(kTag, "Decoded snapshot: tick=%llu entities=%u schema=%u",
             static_cast<unsigned long long>(wire.serverTick),
             wire.entityCount, wire.schemaVersion);

    // Extract entity transforms from component data.
    // TestTransformComponent: float x,y,z, rotX,rotY,rotZ (24 bytes, typeId=1001)
    // TestIdentityComponent: uint32 typeId + char name[32] (36 bytes, typeId=1002)
    for (const auto& entity : wire.entities)
    {
        ClientRep::SnapshotEntityState entityState;
        entityState.entityId = entity.entityId;

        // Find the TestTransformComponent (typeId 1001).
        for (const auto& comp : entity.components)
        {
            if (comp.typeId == 1001 && comp.dataLen >= 24)
            {
                // Extract position from component data.
                float x, y, z;
                std::memcpy(&x, comp.data, 4);
                std::memcpy(&y, comp.data + 4, 4);
                std::memcpy(&z, comp.data + 8, 4);
                entityState.transform = SagaEngine::Math::Transform::FromPosition(
                    SagaEngine::Math::Vec3(x, y, z));
                break;
            }
        }

        outFrame.entities.push_back(entityState);
    }

    outFrame.hasLocalPlayer = (m_config.localPlayerId != 0);
    outFrame.localPlayerId  = m_config.localPlayerId;
    outFrame.ackedInputSeq  = 0;
    return true;
}

bool ClientNetworkSession::DecodeDeltaSnapshot(const std::vector<std::uint8_t>& payload,
                                                 ClientRep::DecodedSnapshotFrame& outFrame)
{
    if (payload.empty())
        return false;

    outFrame.hasLocalPlayer = (m_config.localPlayerId != 0);
    outFrame.localPlayerId  = m_config.localPlayerId;
    outFrame.ackedInputSeq  = 0;
    return true;
}

void ClientNetworkSession::HandleInputAck(const Net::Packet& pkt)
{
    auto serialized = pkt.GetSerializedData();
    constexpr std::size_t kHdrSize = Net::PACKET_HEADER_SIZE;

    if (serialized.size() < kHdrSize + sizeof(uint32_t))
    {
        LOG_WARN(kTag, "InputAck payload too small (%zu bytes).", serialized.size());
        return;
    }

    uint32_t ackedSeq = 0;
    std::memcpy(&ackedSeq, serialized.data() + kHdrSize, sizeof(uint32_t));

#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    ackedSeq = (ackedSeq >> 24) | ((ackedSeq >> 8) & 0xFF00) |
               ((ackedSeq & 0xFF00) << 8) | (ackedSeq << 24);
#endif

    m_inputBuffer.AckUpTo(ackedSeq);
}

bool ClientNetworkSession::SendReliable(Net::PacketType type,
                                          const uint8_t* payload,
                                          std::size_t size)
{
    Net::Packet pkt(type);
    if (payload && size > 0)
        pkt.WriteBytes(payload, size);

    pkt.SetSequence(m_reliableChannel->GetStatistics().packetsSent + 1);
    pkt.SetTimestamp(CurrentTimeMs());
    return m_reliableChannel->Send(std::move(pkt));
}

// ============================================================================
// ClientHost — Application subclass
// ============================================================================

ClientHost::ClientHost(ClientConfig config)
    : Application(config.windowTitle)
    , m_config(std::move(config))
{
}

ClientHost::~ClientHost() = default;

void ClientHost::OnInit()
{
    LOG_INFO(kTag, "ClientHost initialising…");

    ClientSessionConfig sessionConfig;
    sessionConfig.serverHost   = m_config.serverHost;
    sessionConfig.serverPort   = m_config.serverPort;
    sessionConfig.fixedDtSeconds = 1.0f / 60.0f;
    sessionConfig.serverTickHz   = 20.0;

    m_session = std::make_unique<ClientNetworkSession>();
    if (!m_session->Configure(sessionConfig))
    {
        LOG_ERROR(kTag, "Failed to configure client session.");
        RequestClose();
        return;
    }

    if (!m_session->Connect())
    {
        LOG_ERROR(kTag, "Failed to connect to server.");
        RequestClose();
        return;
    }

    m_session->SetOnStateChanged([this](Net::ConnectionState state) {
        LOG_INFO(kTag, "Connection state: %s", Net::ConnectionStateToString(state));
    });

    // ── Debug renderer ──────────────────────────────────────────────────────
    if (!m_config.headless)
    {
        auto* sdlWindow = static_cast<SDL_Window*>(GetWindow()->GetNativeHandle());
        if (sdlWindow)
        {
            m_renderer = SDL_CreateRenderer(sdlWindow, -1,
                                            m_config.vsync ? SDL_RENDERER_PRESENTVSYNC : 0);
            if (!m_renderer)
                LOG_WARN(kTag, "SDL_CreateRenderer failed: %s", SDL_GetError());
        }
    }

    LOG_INFO(kTag, "ClientHost ready.");
}

void ClientHost::OnUpdate()
{
    m_tickCounter++;

    // MMO Client Loop:
    //   1. Recv → Decode → Apply → ECS
    //   2. Send Input → Server
    //   3. Prediction → Reconciliation → Interpolation
    //   4. Render

    m_session->TickReceive();
    m_session->Tick(m_tickCounter);
    TickInput();
    m_session->TickSend();

    if (m_renderer && !m_config.headless)
        RenderDebug();
}

void ClientHost::OnShutdown()
{
    LOG_INFO(kTag, "ClientHost shutting down…");

    if (m_session)
    {
        auto stats = m_session->GetStats();
        LOG_INFO(kTag, "Session stats: frames=%llu, rx=%llu, tx=%llu, "
                 "fullSnap=%llu, deltaSnap=%llu, inputSent=%llu, inputAck=%llu, "
                 "rtt=%.1fms, state=%s",
                 static_cast<unsigned long long>(stats.framesTicked),
                 static_cast<unsigned long long>(stats.packetsReceived),
                 static_cast<unsigned long long>(stats.packetsSent),
                 static_cast<unsigned long long>(stats.fullSnapshotsApplied),
                 static_cast<unsigned long long>(stats.deltaSnapshotsApplied),
                 static_cast<unsigned long long>(stats.inputCommandsSent),
                 static_cast<unsigned long long>(stats.inputAcksReceived),
                 stats.currentRttMs,
                 ClientRep::StateToString(stats.state));

        m_session->Disconnect();
        m_session.reset();
    }

    if (m_renderer)
    {
        SDL_DestroyRenderer(m_renderer);
        m_renderer = nullptr;
    }

    LOG_INFO(kTag, "ClientHost shutdown complete.");
}

void ClientHost::RequestExit()
{
    RequestClose();
}

void ClientHost::TickInput()
{
    const uint8_t* keys = SDL_GetKeyboardState(nullptr);
    if (!keys) return;

    bool anyInput = false;
    int32_t moveX = 0;
    int32_t moveY = 0;
    uint32_t buttons = 0;

    if (keys[SDL_SCANCODE_W]) { moveY += Input::FixedFromFloat(1.0f); anyInput = true; }
    if (keys[SDL_SCANCODE_S]) { moveY -= Input::FixedFromFloat(1.0f); anyInput = true; }
    if (keys[SDL_SCANCODE_A]) { moveX -= Input::FixedFromFloat(1.0f); anyInput = true; }
    if (keys[SDL_SCANCODE_D]) { moveX += Input::FixedFromFloat(1.0f); anyInput = true; }
    if (keys[SDL_SCANCODE_SPACE]) { buttons |= Input::kButtonJump; anyInput = true; }
    if (keys[SDL_SCANCODE_LSHIFT]) { buttons |= Input::kButtonSprint; anyInput = true; }

    if (!anyInput) return;

    m_inputSequence++;
    auto cmd = SagaEngine::Input::MakeInputCommand(
        m_inputSequence,
        static_cast<uint32_t>(m_tickCounter),
        static_cast<uint64_t>(SagaEngine::Core::Time::GetTime() * 1'000'000.0));

    cmd.moveX   = moveX;
    cmd.moveY   = moveY;
    cmd.buttons = buttons;

    m_session->QueueInput(cmd);
}

void ClientHost::RenderDebug()
{
    SDL_SetRenderDrawColor(m_renderer, 30, 30, 35, 255);
    SDL_RenderClear(m_renderer);

    const double renderTime = SagaEngine::Core::Time::GetTime();
    auto transforms = m_session->GetInterpolationManager().SampleAll(renderTime);

    const int windowW = static_cast<int>(GetWindow()->GetWidth());
    const int windowH = static_cast<int>(GetWindow()->GetHeight());
    const int cellSize = 16;

    for (const auto& [entityId, transform] : transforms)
    {
        const float scale = 20.0f;
        int sx = windowW / 2 + static_cast<int>(transform.position.x * scale);
        int sy = windowH / 2 + static_cast<int>(transform.position.z * scale);

        if (entityId == m_session->GetLocalPlayerId())
            SDL_SetRenderDrawColor(m_renderer, 0, 255, 0, 255);
        else
            SDL_SetRenderDrawColor(m_renderer, 0, 200, 255, 255);

        SDL_Rect rect = { sx - cellSize / 2, sy - cellSize / 2, cellSize, cellSize };
        SDL_RenderFillRect(m_renderer, &rect);
    }

    SDL_RenderPresent(m_renderer);
}

} // namespace Saga
