/// @file ZoneServer.h
/// @brief Local-only headless zone server harness for authoritative simulation evidence.
///
/// Layer  : Server / Networking / Server
/// Purpose: ZoneServer owns the authoritative tick loop, coordinates all server
///          subsystems (network transport, simulation, replication, interest
///          management, persistence) and drives the fixed-rate game loop without
///          any rendering or windowing dependency.
///
/// Threading model:
///   - One io_context driven by a configurable thread pool (default: hardware_concurrency).
///   - One dedicated tick thread running the fixed-rate simulation loop.
///   - All subsystem calls from the tick thread are synchronised via strand.
///   - Send I/O is posted onto the io_context from the tick thread; actual socket
///     writes happen on io threads.
///
/// Lifecycle:
///   Initialize() → Run() [blocking] → RequestShutdown() → [Run returns] → Destroy

#pragma once

#include "SagaEngine/Networking/ServerPacketNormalizer.h"
#include "SagaEngine/Replication/MovementDirtyReplicationBridge.h"
#include "SagaEngine/Replication/ReplicationManager.h"
#include "SagaEngine/Replication/Interest/InterestArea.h"
#include "SagaEngine/Networking/NetworkTypes.h"
#include "SagaEngine/ServerAuthority/Simulation/ActorOwnershipRegistry.h"
#include "SagaEngine/ServerAuthority/Simulation/AuthoritativeMovementCommandIntake.h"
#include "SagaEngine/Simulation/SimulationTick.h"
#include "SagaEngine/Simulation/WorldState.h"
#include "SagaEngine/Core/Threading/JobSystem.h"
#include "SagaEngine/Core/Log/Log.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

namespace SagaEngine::Diagnostics
{
class DiagnosticSystem;
}

namespace SagaEngine::ServerAuthority
{

using SagaEngine::Networking::ClientId;

// ─── Forward declarations ─────────────────────────────────────────────────────

class ShardManager;

// ─── ZoneServerConfig ─────────────────────────────────────────────────────────

/// Full runtime configuration for one zone server instance.
struct ZoneServerConfig
{
    // ── Network ───────────────────────────────────────────────────────────────

    std::string bindAddress{"127.0.0.1"};     ///< Loopback only; public binds fail closed
    uint16_t    port{7777};                    ///< UDP/TCP listen port
    uint32_t    maxClients{1000};              ///< Hard client cap enforced at accept
    uint32_t    ioThreadCount{0};              ///< 0 = hardware_concurrency()

    // ── Simulation ────────────────────────────────────────────────────────────

    uint32_t    tickRateHz{64};                ///< Fixed simulation tick rate
    uint32_t    replicationRateHz{20};         ///< Max replication snapshot rate
    float       catchupMaxMultiplier{4.0f};    ///< Spiral-of-death guard multiplier

    // ── Zone identity ─────────────────────────────────────────────────────────

    uint32_t    zoneId{0};                     ///< Unique zone identifier in shard mesh
    uint32_t    shardId{0};                    ///< Shard this zone belongs to
    std::string zoneName{"default"};           ///< Human-readable zone name for logs

    // ── Health & diagnostics ──────────────────────────────────────────────────

    uint32_t    statsIntervalMs{5000};         ///< Periodic stats log interval
    bool        enableDetailedPacketLog{false};///< Log every packet (dev only)
    uint32_t    maxTickBudgetUs{14000};        ///< Soft budget warning threshold (µs)
};

// ─── ZoneServerStats ──────────────────────────────────────────────────────────

/// Live metrics snapshot, thread-safe via atomic reads.
struct ZoneServerStats
{
    std::atomic<uint64_t> totalTicksProcessed{0};
    std::atomic<uint64_t> totalPacketsSent{0};
    std::atomic<uint64_t> totalPacketsReceived{0};
    std::atomic<uint64_t> totalPacketsNormalized{0};
    std::atomic<uint64_t> totalPacketsRejected{0};
    std::atomic<uint64_t> totalBytesSent{0};
    std::atomic<uint64_t> totalBytesReceived{0};
    std::atomic<uint32_t> currentClientCount{0};
    std::atomic<uint64_t> totalTickBudgetOverruns{0};
    std::atomic<uint64_t> lastTickDurationUs{0};
    std::atomic<uint64_t> averageTickDurationUs{0};
    std::atomic<uint64_t> peakTickDurationUs{0};
};

// ─── IZoneServerListener ──────────────────────────────────────────────────────

/// Observer interface for zone lifecycle and session events.
class IZoneServerListener
{
public:
    virtual ~IZoneServerListener() = default;

    virtual void OnServerStarted(uint32_t zoneId, uint16_t port)           {}
    virtual void OnServerStopped(uint32_t zoneId)                          {}
    virtual void OnClientConnected(ClientId clientId, uint32_t zoneId)     {}
    virtual void OnClientDisconnected(ClientId clientId, uint32_t zoneId)  {}
    /// The packet payload is non-owning and valid only for the duration of
    /// this callback.
    virtual void OnInboundPacketNormalized(
        const ::SagaEngine::Networking::NormalizedServerPacketView&)       {}
    virtual void OnTickCompleted(uint64_t tick, uint64_t durationUs)       {}
    virtual void OnTickBudgetOverrun(uint64_t tick, uint64_t durationUs)   {}
};

// ─── ZoneServer ───────────────────────────────────────────────────────────────

/// Authoritative headless zone server — owns the simulation tick loop and all
/// server subsystems for one zone partition.
class ZoneServer final
{
public:
    explicit ZoneServer(ZoneServerConfig config);
    ~ZoneServer();

    ZoneServer(const ZoneServer&)            = delete;
    ZoneServer& operator=(const ZoneServer&) = delete;
    ZoneServer(ZoneServer&&)                 = delete;
    ZoneServer& operator=(ZoneServer&&)      = delete;

    // ── Lifecycle ─────────────────────────────────────────────────────────────

    /// Initialize all subsystems. Must be called before Run().
    /// Returns false and logs the reason on any subsystem init failure.
    [[nodiscard]] bool Initialize();

    /// Block the calling thread running the io_context + tick loop until
    /// RequestShutdown() is called or a fatal error occurs.
    void Run();

    /// Signal graceful shutdown from any thread. Run() will return after the
    /// current tick and all in-flight sends are flushed.
    void RequestShutdown();

    /// Tear down all subsystems. Safe to call multiple times.
    void Destroy();

    /// Attach optional diagnostics. The server does not own this pointer.
    void SetDiagnostics(
        SagaEngine::Diagnostics::DiagnosticSystem* diagnostics) noexcept;

    // ── Queries ───────────────────────────────────────────────────────────────

    [[nodiscard]] bool     IsRunning()           const noexcept;
    [[nodiscard]] uint32_t GetZoneId()           const noexcept;
    [[nodiscard]] uint16_t GetBoundPort()        const noexcept;
    [[nodiscard]] uint32_t GetCurrentClientCount() const noexcept;
    [[nodiscard]] const ZoneServerStats& GetStats() const noexcept;
    [[nodiscard]] const ZoneServerConfig& GetConfig() const noexcept;

    // ── Subsystem access ──────────────────────────────────────────────────────

    /// Direct access to the world state for external system integration.
    [[nodiscard]] SagaEngine::Simulation::WorldState* GetWorldState() noexcept;

    /// Direct access to the replication manager for component registration.
    [[nodiscard]] SagaEngine::Replication::ReplicationManager*
        GetReplicationManager() noexcept;

    // ── Controlled actor ownership ───────────────────────────────────────────

    [[nodiscard]] SagaEngine::ServerAuthority::Simulation::ActorOwnershipResult
        RegisterControlledActor(
            ClientId clientId,
            SagaEngine::ServerAuthority::Simulation::EntityId entityId,
            SagaEngine::ServerAuthority::Simulation::Vector3 initialPosition);

    [[nodiscard]] SagaEngine::ServerAuthority::Simulation::ActorOwnershipResult
        UnregisterControlledActor(ClientId clientId);

    [[nodiscard]] std::optional<SagaEngine::ServerAuthority::Simulation::ControlledActor>
        FindControlledActor(ClientId clientId) const;

    // ── Observer ──────────────────────────────────────────────────────────────

    void AddListener(IZoneServerListener* listener);
    void RemoveListener(IZoneServerListener* listener);

    // ── Tick injection for testing ─────────────────────────────────────────────

    /// Force one simulation tick; used by stress tests and sandbox scenarios.
    /// Must not be called while Run() is active.
    void ForceTick(double wallDeltaSeconds);

    /// Exercise the same raw inbound packet normalization path used by the
    /// tick drain without requiring live sockets.
    [[nodiscard]] ::SagaEngine::Networking::ServerPacketNormalizationStatus
        ProcessRawInboundPacketForTesting(
        ClientId clientId, const std::uint8_t* data, std::size_t size);

    [[nodiscard]] SagaEngine::ServerAuthority::Simulation::AuthoritativeMovementTickReport
        TickMovementForTesting(uint64_t tickIndex, double fixedDelta);

    [[nodiscard]] std::optional<SagaEngine::ServerAuthority::Simulation::Vector3>
        GetControlledActorPositionForTesting(
            SagaEngine::ServerAuthority::Simulation::EntityId entityId) const;

    [[nodiscard]] std::optional<
        SagaEngine::ServerAuthority::Simulation::AuthoritativeMovementCommandIntakeResult>
        GetLastMovementIntakeResultForTesting() const;

    [[nodiscard]] std::vector<
        SagaEngine::Replication::MovementDirtyReplicationIntent>
        ConsumeMovementDirtyReplicationIntentsForTesting();

    [[nodiscard]] std::size_t
        GetPendingMovementDirtyReplicationIntentCountForTesting() const;

    /// Record a supplied tick duration through diagnostics for deterministic tests.
    void RecordTickDurationForTesting(uint64_t tickIndex, uint64_t durationUs);
    /// Record server-start lifecycle diagnostics without starting sockets or threads.
    void RecordServerStartedForTesting();
    /// Record session connection lifecycle diagnostics without live sockets.
    void RecordSessionConnectedForTesting(ClientId clientId);
    /// Record session disconnection lifecycle diagnostics without live sockets.
    void RecordSessionDisconnectedForTesting(ClientId clientId);

private:
    // ── Internal subsystem types (fwd declared to hide headers) ───────────────

    struct NetworkRuntime;

    // ── Init helpers ──────────────────────────────────────────────────────────

    [[nodiscard]] bool InitNetworking();
    [[nodiscard]] bool InitSimulation();
    [[nodiscard]] bool InitReplication();
    [[nodiscard]] bool InitInterest();
    [[nodiscard]] bool InitJobSystem();

    // ── Tick loop ─────────────────────────────────────────────────────────────

    void TickLoop();
    void ExecuteTick(uint64_t tickIndex, double fixedDelta);
    void DrainInputPackets();
    [[nodiscard]] ::SagaEngine::Networking::ServerPacketNormalizationStatus
        ProcessRawInboundPacket(
        ClientId clientId, const std::uint8_t* data, std::size_t size);
    void HandleNormalizedInboundPacket(
        const ::SagaEngine::Networking::NormalizedServerPacketView& packet);
    void StepSimulation(uint64_t tickIndex, double fixedDelta);
    [[nodiscard]] SagaEngine::ServerAuthority::Simulation::AuthoritativeMovementTickReport
        TickMovementAuthority(uint64_t tickIndex, double fixedDelta);
    void FlushReplication(uint64_t tickIndex);
    void PumpNetworkIO();

    // ── Stats ─────────────────────────────────────────────────────────────────

    void UpdateTickStats(uint64_t durationUs);
    void LogPeriodicStats();

    // ── Listener dispatch ─────────────────────────────────────────────────────

    void NotifyServerStarted();
    void NotifyServerStopped();
    void NotifyClientConnected(ClientId clientId);
    void NotifyClientDisconnected(ClientId clientId);
    void NotifyInboundPacketNormalized(
        const ::SagaEngine::Networking::NormalizedServerPacketView& packet);
    void NotifyTickCompleted(uint64_t tick, uint64_t durationUs);

    // ── Optional diagnostics ─────────────────────────────────────────────────

    void RecordDiagnosticCounter(const char* name, double amount = 1.0);
    void RecordDiagnosticGauge(const char* name, double value);
    void RecordDiagnosticTiming(const char* name, double milliseconds);
    void RecordActiveControlledActorCount();
    void RecordMovementIntakeDiagnostics(
        const SagaEngine::ServerAuthority::Simulation::
            AuthoritativeMovementCommandIntakeResult& result);
    void RecordTickReliabilityDiagnostics(uint64_t tickIndex, uint64_t durationUs);
    void RecordLifecycleEvent(
        std::string eventName,
        std::string category,
        std::string severity,
        std::string message,
        uint64_t tick = 0,
        std::vector<std::pair<std::string, std::string>> payload = {});
    void RecordServerStartedLifecycle(uint64_t tick = 0);
    void RecordServerStoppedLifecycle(uint64_t tick = 0);
    void RecordShutdownRequestedLifecycle(uint64_t tick = 0);
    void RecordSessionConnectedLifecycle(ClientId clientId, uint64_t tick = 0);
    void RecordSessionDisconnectedLifecycle(ClientId clientId, uint64_t tick = 0);
    void RecordEntityCreatedLifecycle(
        ClientId clientId,
        SagaEngine::ServerAuthority::Simulation::EntityId entityId,
        uint64_t tick = 0);
    void RecordEntityDestroyedLifecycle(
        ClientId clientId,
        SagaEngine::ServerAuthority::Simulation::EntityId entityId,
        uint64_t tick = 0);
    [[nodiscard]] uint64_t FindSessionLifecycleRecordId(ClientId clientId) const;

    // ── State ─────────────────────────────────────────────────────────────────

    ZoneServerConfig m_config;
    ZoneServerStats  m_stats;
    SagaEngine::Diagnostics::DiagnosticSystem* m_diagnostics{nullptr};

    std::atomic<bool>     m_running{false};
    std::atomic<bool>     m_shutdownRequested{false};
    std::atomic<uint16_t> m_boundPort{0};

    // ── Network runtime ───────────────────────────────────────────────────────

    std::unique_ptr<NetworkRuntime> m_networkRuntime;

    // ── Subsystems ────────────────────────────────────────────────────────────

    std::unique_ptr<SagaEngine::Simulation::SimulationTick>                   m_simTick;
    std::unique_ptr<SagaEngine::Simulation::WorldState>                       m_worldState;
    std::unique_ptr<SagaEngine::Replication::ReplicationManager> m_replicationManager;
    std::unique_ptr<SagaEngine::Replication::Interest::InterestManager>        m_interestManager;
    std::unique_ptr<SagaEngine::ServerAuthority::Simulation::ActorOwnershipRegistry>   m_actorOwnershipRegistry;
    std::unique_ptr<SagaEngine::ServerAuthority::Simulation::AuthoritativeMovementCommandIntake> m_movementCommandIntake;
    std::unique_ptr<SagaEngine::Replication::MovementDirtyReplicationBridge> m_movementDirtyReplicationBridge;

    // ── Tick loop state ───────────────────────────────────────────────────────

    std::thread  m_tickThread;
    mutable std::mutex m_listenerMutex;
    std::vector<IZoneServerListener*> m_listeners;
    mutable std::mutex m_movementAuthorityMutex;
    mutable std::mutex m_lifecycleRecordMutex;
    std::optional<SagaEngine::ServerAuthority::Simulation::AuthoritativeMovementCommandIntakeResult>
        m_lastMovementIntakeResult;
    SagaEngine::ServerAuthority::Simulation::AuthoritativeMovementTickReport
        m_lastMovementTickReport;
    uint64_t m_serverLifecycleRecordId{0};
    std::unordered_map<ClientId, uint64_t> m_sessionLifecycleRecordIds;
    std::unordered_map<ClientId, uint64_t> m_entityLifecycleRecordIdsByClient;

    // ── Stats accumulation ────────────────────────────────────────────────────

    uint64_t m_tickDurationAccumUs{0};
    uint64_t m_tickDurationSampleCount{0};
    std::chrono::steady_clock::time_point m_lastStatsLog;

    static constexpr uint64_t kTickDurationWindowSize{256};
};

} // namespace SagaEngine::ServerAuthority
