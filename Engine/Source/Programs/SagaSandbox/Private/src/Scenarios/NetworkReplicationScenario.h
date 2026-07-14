/// @file NetworkReplicationScenario.h
/// @brief In-process server+client replication stress scenario.
///
/// Layer  : Sandbox / Scenarios / Catalog
/// Purpose: Runs an authoritative server loop and a client prediction loop
///          inside the same process, communicating via loopback UDP.
///          Tests the full MMO data flow:
///
///   Client input → InputCommand → Server inbox → SimulationTick →
///   ReplicationManager → Packet → Client reconcile → ClientPrediction
///
/// What this scenario exercises:
///   - InputCommandInbox + ServerInputProcessor per-tick flow
///   - ReplicationManager dirty-marking and delta send
///   - InterestArea enter/leave detection
///   - ClientPrediction divergence detection and rollback
///   - ReliableChannel RTT and loss statistics
///   - Snapshot / event log persistence (optional, configurable)

#pragma once

#include <SagaSandbox/Core/IScenario.h>
#include <SagaEngine/Input/Networking/ServerInputProcessor.h>
#include <SagaEngine/Input/Networking/ClientPrediction.h>
#include <SagaEngine/Input/Commands/InputCommandBuffer.h>
#include <SagaEngine/Simulation/WorldState.h>
#include <SagaEngine/Simulation/SimulationTick.h>
#include <SagaServer/Networking/Replication/ReplicationManager.h>
#include <SagaServer/Networking/Core/ReliableChannel.h>
#include <memory>

namespace SagaSandbox
{

class NetworkReplicationScenario final : public IScenario
{
public:
    static constexpr ScenarioDescriptor kDescriptor
    {
        .id          = "network_replication",
        .displayName = "Network Replication",
        .category    = "Network",
        .description = "In-process authoritative server + client replication over loopback UDP.",
    };

    NetworkReplicationScenario()  = default;
    ~NetworkReplicationScenario() override = default;

    [[nodiscard]] const ScenarioDescriptor& GetDescriptor() const noexcept override
    {
        return kDescriptor;
    }

    [[nodiscard]] bool OnInit()                          override;
    void               OnUpdate(float dt, uint64_t tick) override;
    void               OnShutdown()                      override;
    void               OnRenderDebugUI()                 override;

private:
    // ── Server side ──────────────────────────────────────────────────────────

    void TickServer(float dt, uint64_t tick);

    std::unique_ptr<SagaEngine::Simulation::WorldState>         m_serverWorld;
    std::unique_ptr<SagaEngine::Simulation::SimulationTick>     m_serverSimTick;
    std::unique_ptr<SagaEngine::Input::ServerInputProcessor> m_serverInputProcessor;
    std::unique_ptr<SagaEngine::Networking::Replication::ReplicationManager> m_replicationManager;
    std::unique_ptr<SagaEngine::Networking::ReliableChannel>    m_serverChannel;

    // ── Client side ───────────────────────────────────────────────────────────

    void TickClient(float dt, uint64_t tick);

    SagaEngine::Input::InputCommandBuffer  m_clientCommandBuffer;
    std::unique_ptr<SagaEngine::Networking::ReliableChannel> m_clientChannel;

    uint32_t m_clientSequence  = 0;
    uint32_t m_simulatedClients = 4;  ///< Configurable via HUD slider

    // ── HUD state ─────────────────────────────────────────────────────────────

    void RenderServerPanel();
    void RenderClientPanel();
    void RenderReplicationPanel();
    void RenderReliableChannelPanel();

    struct FrameStats
    {
        uint64_t commandsSent      = 0;
        uint64_t commandsProcessed = 0;
        uint64_t entitiesReplicated = 0;
        float    avgRttMs          = 0.f;
        float    packetLossRate    = 0.f;
        uint32_t predictionRollbacks = 0;
    };
    FrameStats m_stats;
};

} // namespace SagaSandbox