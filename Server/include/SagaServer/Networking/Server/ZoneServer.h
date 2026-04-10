/// @file ZoneServer.h
/// @brief Production-grade headless zone server — the authoritative simulation host.
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

#include "SagaServer/Networking/Client/ConnectionManager.h"
#include "SagaServer/Networking/Replication/ReplicationManager.h"
#include "SagaServer/Networking/Interest/InterestArea.h"
#include "SagaServer/Networking/Core/NetworkTypes.h"
#include "SagaEngine/Simulation/SimulationTick.h"
#include "SagaEngine/Simulation/WorldState.h"
#include "SagaEngine/Core/Threading/JobSystem.h"
#include "SagaEngine/Core/Log/Log.h"

#include <asio.hpp>
#include <asio/steady_timer.hpp>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace SagaServer::Networking
{

// ─── Forward declarations ─────────────────────────────────────────────────────

class ShardManager;

// ─── ZoneServerConfig ─────────────────────────────────────────────────────────

/// Full runtime configuration for one zone server instance.
struct ZoneServerConfig
{
    // ── Network ───────────────────────────────────────────────────────────────

    std::string bindAddress{"0.0.0.0"};       ///< Listen interface
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
    [[nodiscard]] SagaEngine::Networking::Replication::ReplicationManager*
        GetReplicationManager() noexcept;

    // ── Observer ──────────────────────────────────────────────────────────────

    void AddListener(IZoneServerListener* listener);
    void RemoveListener(IZoneServerListener* listener);

    // ── Tick injection for testing ─────────────────────────────────────────────

    /// Force one simulation tick; used by stress tests and sandbox scenarios.
    /// Must not be called while Run() is active.
    void ForceTick(double wallDeltaSeconds);

private:
    // ── Internal subsystem types (fwd declared to hide headers) ───────────────

    struct AcceptContext;

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
    void StepSimulation(uint64_t tickIndex, double fixedDelta);
    void FlushReplication(uint64_t tickIndex);
    void PumpNetworkIO();

    // ── Accept loop ───────────────────────────────────────────────────────────

    void StartAccept();
    void HandleAccept(const asio::error_code& ec,
                      asio::ip::tcp::socket   socket);

    // ── Stats ─────────────────────────────────────────────────────────────────

    void UpdateTickStats(uint64_t durationUs);
    void LogPeriodicStats();

    // ── Listener dispatch ─────────────────────────────────────────────────────

    void NotifyServerStarted();
    void NotifyServerStopped();
    void NotifyClientConnected(ClientId clientId);
    void NotifyClientDisconnected(ClientId clientId);
    void NotifyTickCompleted(uint64_t tick, uint64_t durationUs);

    // ── State ─────────────────────────────────────────────────────────────────

    ZoneServerConfig m_config;
    ZoneServerStats  m_stats;

    std::atomic<bool>     m_running{false};
    std::atomic<bool>     m_shutdownRequested{false};
    std::atomic<uint16_t> m_boundPort{0};

    // ── Asio runtime ─────────────────────────────────────────────────────────

    asio::io_context                              m_ioContext;
    asio::executor_work_guard<
        asio::io_context::executor_type>          m_workGuard;
    std::unique_ptr<asio::ip::tcp::acceptor>      m_acceptor;
    std::vector<std::thread>                      m_ioThreads;

    // ── Subsystems ────────────────────────────────────────────────────────────

    std::unique_ptr<SagaEngine::Simulation::SimulationTick>                   m_simTick;
    std::unique_ptr<SagaEngine::Simulation::WorldState>                       m_worldState;
    std::unique_ptr<ConnectionManager>                                        m_connectionManager;
    std::unique_ptr<SagaEngine::Networking::Replication::ReplicationManager> m_replicationManager;
    std::unique_ptr<SagaEngine::Networking::Interest::InterestManager>        m_interestManager;

    // ── Tick loop state ───────────────────────────────────────────────────────

    std::thread  m_tickThread;
    mutable std::mutex m_listenerMutex;
    std::vector<IZoneServerListener*> m_listeners;

    // ── Stats accumulation ────────────────────────────────────────────────────

    uint64_t m_tickDurationAccumUs{0};
    uint64_t m_tickDurationSampleCount{0};
    std::chrono::steady_clock::time_point m_lastStatsLog;

    static constexpr uint64_t kTickDurationWindowSize{256};
};

} // namespace SagaServer::Networking
