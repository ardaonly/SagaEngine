/// @file NetworkReplicationScenario.cpp
/// @brief In-process server+client replication scenario over loopback.
///
/// API contract fixes applied (2026-04-07):
///
///   SimulationTick:
///     - Constructor: SimulationTick(uint32_t fixedHz) — no config struct
///     - No SetWorldState()  — SimulationTick is a pure time accumulator
///     - Tick()  → Advance(double wallDeltaSeconds)
///     - GetTickCount() → CurrentTick()
///
///   WorldState:
///     - GetAllEntities() → GetAliveEntities()
///     - world.Query({}) → m_serverWorld->GetAliveEntities()

#include "NetworkReplicationScenario.h"
#include "SagaSandbox/Core/ScenarioRegistry.h"

#include <SagaEngine/Core/Log/Log.h>
#include <SagaEngine/Simulation/Authority.h>
#include <SagaEngine/ECS/ComponentRegistry.h>
#include <SagaEngine/ECS/ComponentSerializerRegistry.h>
#include <SagaEngine/Input/Commands/InputCommand.h>

#include <imgui.h>
#include <chrono>
#include <cmath>
#include <cstring>

namespace SagaSandbox
{

SAGA_SANDBOX_REGISTER_SCENARIO(NetworkReplicationScenario);

using namespace SagaEngine;
using namespace SagaEngine::Networking;
using namespace SagaEngine::Networking::Replication;
using namespace SagaEngine::Networking::Interest;
using SagaEngine::Input::ServerInputProcessor;
using SagaEngine::Input::InputValidationConfig;

static constexpr const char* kTag = "NetworkReplication";

// ─── Sandbox network component ────────────────────────────────────────────────
//
// SbNetPosition lives in [2001, 2099] — the Network scenario component range.
// Declared in anonymous namespace to avoid ODR collision if other TUs include
// this header through a transitive path.
//
struct SbNetPosition { float x = 0.f, y = 0.f; };

/// Stable ID for SbNetPosition. Must not collide with any engine component ID.
static constexpr SagaEngine::ECS::ComponentTypeId kIdSbNetPosition = 2001u;

// ─── Registration helper ──────────────────────────────────────────────────────

static void RegisterNetworkComponents()
{
    using CR  = SagaEngine::ECS::ComponentRegistry;
    using CSR = SagaEngine::ECS::ComponentSerializerRegistry;

    if (!CR::Instance().IsRegistered<SbNetPosition>())
    {
        CR::Instance().Register<SbNetPosition>(kIdSbNetPosition, "SbNetPosition");

        CSR::Instance().Register<SbNetPosition>(
            kIdSbNetPosition,
            "SbNetPosition",
            [](const SbNetPosition& d, void* buf, size_t sz) -> size_t {
                if (sz < sizeof(SbNetPosition)) return 0;
                std::memcpy(buf, &d, sizeof(SbNetPosition));
                return sizeof(SbNetPosition);
            },
            [](SbNetPosition& d, const void* buf, size_t sz) -> size_t {
                if (sz < sizeof(SbNetPosition)) return 0;
                std::memcpy(&d, buf, sizeof(SbNetPosition));
                return sizeof(SbNetPosition);
            },
            []() -> size_t { return sizeof(SbNetPosition); }
        );
    }
}

// ─── Lifecycle ────────────────────────────────────────────────────────────────

bool NetworkReplicationScenario::OnInit()
{
    LOG_INFO(kTag, "Initialising network replication scenario…");

    // ── Register components ───────────────────────────────────────────────────
    //
    // Must happen before any WorldState::AddComponent call.

    RegisterNetworkComponents();

    // ── Server world ──────────────────────────────────────────────────────────

    m_serverWorld = std::make_unique<Simulation::WorldState>();

    // SimulationTick takes a fixed Hz rate — not a config struct.
    // 30 Hz authoritative server tick rate.
    m_serverSimTick = std::make_unique<Simulation::SimulationTick>(30u);

    // SimulationTick is a pure time accumulator. It does NOT own WorldState.
    // WorldState is driven externally each time Advance() returns ticks > 0.

    // ── Server input processor ────────────────────────────────────────────────

    InputValidationConfig valCfg;
    m_serverInputProcessor = std::make_unique<ServerInputProcessor>(valCfg);

    for (uint32_t i = 0; i < m_simulatedClients; ++i)
        m_serverInputProcessor->AddClient(i + 1);

    // ── Replication manager ───────────────────────────────────────────────────

    m_replicationManager = std::make_unique<ReplicationManager>();
    m_replicationManager->Initialize(m_simulatedClients + 1);
    m_replicationManager->SetWorldState(m_serverWorld.get());
    m_replicationManager->SetReplicationFrequency(20.f);

    for (uint32_t i = 0; i < m_simulatedClients; ++i)
        m_replicationManager->AddClient(static_cast<ClientId>(i + 1));

    // ── Spawn server entities ─────────────────────────────────────────────────

    for (uint32_t i = 0; i < 16; ++i)
    {
        const auto entity = m_serverWorld->CreateEntity();
        SbNetPosition pos{ static_cast<float>(i) * 5.f, 0.f };
        m_serverWorld->AddComponent<SbNetPosition>(entity.id, pos);
        m_replicationManager->RegisterEntityForReplication(entity.id);
    }

    // ── ReliableChannels (loopback pair) ──────────────────────────────────────

    ReliableChannelConfig chanCfg;
    chanCfg.windowSize         = 64;
    chanCfg.maxRetransmissions = 3;
    chanCfg.rttTimeoutMs       = 50;

    m_serverChannel = std::make_unique<ReliableChannel>(static_cast<ConnectionId>(1));
    m_serverChannel->Initialize(chanCfg);
    m_serverChannel->SetConnected(true);

    m_clientChannel = std::make_unique<ReliableChannel>(static_cast<ConnectionId>(2));
    m_clientChannel->Initialize(chanCfg);
    m_clientChannel->SetConnected(true);

    LOG_INFO(kTag, "Network replication scenario ready (%u simulated clients).",
             m_simulatedClients);
    return true;
}

void NetworkReplicationScenario::OnUpdate(float dt, uint64_t tick)
{
    TickClient(dt, tick);
    TickServer(dt, tick);

    // ── Loopback exchange: client → server ────────────────────────────────────
    {
        auto clientToServer = m_clientChannel->GetPacketsToSend();
        for (auto& pkt : clientToServer)
        {
            auto data = pkt.GetSerializedData();
            m_serverChannel->Receive(data.data(), data.size());
        }
    }

    // ── Loopback exchange: server → client ────────────────────────────────────
    {
        auto serverToClient = m_serverChannel->GetPacketsToSend();
        for (auto& pkt : serverToClient)
        {
            auto data = pkt.GetSerializedData();
            m_clientChannel->Receive(data.data(), data.size());
        }
    }

    // ── Replication: gather dirty, send deltas ────────────────────────────────

    m_replicationManager->GatherDirtyEntities(dt);

    for (uint32_t i = 0; i < m_simulatedClients; ++i)
    {
        const ClientId cid = static_cast<ClientId>(i + 1);
        m_replicationManager->SendUpdates(cid,
            [this](const uint8_t* data, size_t size)
            {
                // Loopback: bytes would go over the wire in a real scenario.
                m_stats.entitiesReplicated++;
                (void)data; (void)size;
            }
        );
    }

    // ── Channel stats ─────────────────────────────────────────────────────────
    {
        const auto cs      = m_serverChannel->GetStatistics();
        m_stats.avgRttMs       = cs.averageRttMs;
        m_stats.packetLossRate = cs.packetLossRate;
    }
}

void NetworkReplicationScenario::TickServer(float dt, uint64_t tick)
{
    // Process input from all fake clients for this tick
    auto result = m_serverInputProcessor->ProcessTick(
        static_cast<uint32_t>(tick));

    m_stats.commandsProcessed += static_cast<uint64_t>(result.entries.size());

    // Advance the simulation tick accumulator.
    // Advance() returns how many fixed steps are ready; we consume them here.
    const uint32_t steps = m_serverSimTick->Advance(static_cast<double>(dt));
    (void)steps; // Systems would run once per step in production.

    // Move entities in circles and mark them dirty for replication.
    // GetAliveEntities() is the correct API — WorldState has no GetAllEntities().
    const auto& alive = m_serverWorld->GetAliveEntities();
    for (const auto eid : alive)
    {
        auto* pos = m_serverWorld->GetComponent<SbNetPosition>(eid);
        if (!pos) continue;

        pos->x = std::cos(static_cast<float>(tick) * 0.02f + static_cast<float>(eid) * 0.5f) * 10.f;
        pos->y = std::sin(static_cast<float>(tick) * 0.02f + static_cast<float>(eid) * 0.5f) * 10.f;

        m_replicationManager->MarkComponentDirty(
            eid, ECS::ComponentRegistry::Instance().GetId<SbNetPosition>());
    }
}

void NetworkReplicationScenario::TickClient(float dt, uint64_t tick)
{
    // Fake client: push one input command per tick for client #1
    auto cmd = Input::MakeInputCommand(
        ++m_clientSequence,
        static_cast<uint32_t>(tick),
        0
    );
    cmd.moveX = Input::FixedFromFloat(std::cos(static_cast<float>(tick) * 0.03f));
    cmd.moveY = Input::FixedFromFloat(std::sin(static_cast<float>(tick) * 0.03f));

    m_clientCommandBuffer.PushLocal(cmd);
    m_stats.commandsSent++;

    // Route command directly to server inbox (loopback, no network serialization)
    auto* inbox = m_serverInputProcessor->GetInbox(1);
    if (inbox) inbox->PushReceived(cmd);

    (void)dt;
}

void NetworkReplicationScenario::OnShutdown()
{
    m_replicationManager->Shutdown();
    m_replicationManager.reset();
    m_serverInputProcessor.reset();
    m_serverSimTick.reset();
    m_serverWorld.reset();
    m_serverChannel.reset();
    m_clientChannel.reset();
    m_clientCommandBuffer.Reset();

    LOG_INFO(kTag, "Network replication scenario shut down. "
                   "cmds_sent=%llu replicated=%llu",
             static_cast<unsigned long long>(m_stats.commandsSent),
             static_cast<unsigned long long>(m_stats.entitiesReplicated));
}

// ─── Debug UI ─────────────────────────────────────────────────────────────────

void NetworkReplicationScenario::OnRenderDebugUI()
{
    RenderServerPanel();
    RenderClientPanel();
    RenderReplicationPanel();
    RenderReliableChannelPanel();
}

void NetworkReplicationScenario::RenderServerPanel()
{
    if (!ImGui::Begin("Server")) { ImGui::End(); return; }

    ImGui::SeparatorText("Simulation");

    // GetAliveEntities() — GetAllEntities() does not exist on WorldState.
    ImGui::Text("Entities : %zu",
                m_serverWorld->GetAliveEntities().size());

    // CurrentTick() — GetTickCount() does not exist on SimulationTick.
    ImGui::Text("Tick     : %llu",
                static_cast<unsigned long long>(m_serverSimTick->CurrentTick()));

    ImGui::SeparatorText("Input Processing");
    ImGui::Text("Clients registered : %zu",
                m_serverInputProcessor->GetConnectedClientCount());
    ImGui::Text("Commands processed : %llu",
                static_cast<unsigned long long>(m_stats.commandsProcessed));

    // Runtime slider: add/remove simulated clients
    int clients = static_cast<int>(m_simulatedClients);
    if (ImGui::SliderInt("Simulated clients", &clients, 1, 32))
    {
        const uint32_t newCount = static_cast<uint32_t>(clients);
        if (newCount > m_simulatedClients)
        {
            for (uint32_t i = m_simulatedClients; i < newCount; ++i)
            {
                m_serverInputProcessor->AddClient(i + 1);
                m_replicationManager->AddClient(static_cast<ClientId>(i + 1));
            }
        }
        else
        {
            for (uint32_t i = newCount; i < m_simulatedClients; ++i)
            {
                m_serverInputProcessor->RemoveClient(i + 1);
                m_replicationManager->RemoveClient(static_cast<ClientId>(i + 1));
            }
        }
        m_simulatedClients = newCount;
    }

    ImGui::End();
}

void NetworkReplicationScenario::RenderClientPanel()
{
    if (!ImGui::Begin("Client")) { ImGui::End(); return; }

    ImGui::Text("Commands sent    : %llu",
                static_cast<unsigned long long>(m_stats.commandsSent));
    ImGui::Text("Unacked cmds     : %zu",
                m_clientCommandBuffer.GetUnackedCount());
    ImGui::Text("Pending cmds     : %zu",
                m_clientCommandBuffer.GetPendingCount());

    ImGui::End();
}

void NetworkReplicationScenario::RenderReplicationPanel()
{
    if (!ImGui::Begin("Replication")) { ImGui::End(); return; }

    ImGui::Text("Entities replicated : %llu",
                static_cast<unsigned long long>(m_stats.entitiesReplicated));
    ImGui::Text("Replication Hz      : %.1f",
                m_replicationManager->GetReplicationFrequency());

    float hz = m_replicationManager->GetReplicationFrequency();
    if (ImGui::SliderFloat("Rep Hz", &hz, 1.f, 60.f))
        m_replicationManager->SetReplicationFrequency(hz);

    ImGui::End();
}

void NetworkReplicationScenario::RenderReliableChannelPanel()
{
    if (!ImGui::Begin("Channel Stats")) { ImGui::End(); return; }

    const auto cs = m_serverChannel->GetStatistics();

    ImGui::Text("RTT          : %.2f ms",  cs.averageRttMs);
    ImGui::Text("Jitter       : %.2f ms",  cs.jitterMs);
    ImGui::Text("Loss rate    : %.1f%%",   cs.packetLossRate * 100.f);
    ImGui::Text("Sent         : %llu",     static_cast<unsigned long long>(cs.packetsSent));
    ImGui::Text("Acked        : %llu",     static_cast<unsigned long long>(cs.packetsAcknowledged));
    ImGui::Text("Lost         : %llu",     static_cast<unsigned long long>(cs.packetsLost));
    ImGui::Text("Retransmit   : %llu",     static_cast<unsigned long long>(cs.packetsRetransmitted));

    ImGui::End();
}

} // namespace SagaSandbox