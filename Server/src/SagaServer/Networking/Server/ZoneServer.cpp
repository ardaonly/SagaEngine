/// @file ZoneServer.cpp
/// @brief ZoneServer — production-grade authoritative headless zone server.

#include "SagaServer/Networking/Server/ZoneServer.h"
#include "SagaServer/Networking/Client/ConnectionManager.h"
#include "SagaServer/Networking/Core/NetworkTypes.h"
#include "SagaEngine/Simulation/SimulationTick.h"
#include "SagaEngine/Simulation/WorldState.h"
#include "SagaEngine/Core/Log/Log.h"

#include <asio.hpp>
#include <asio/steady_timer.hpp>

#include <algorithm>
#include <cassert>
#include <chrono>
#include <thread>

namespace SagaServer::Networking
{

using namespace std::chrono_literals;

static constexpr const char* kTag = "ZoneServer";

// ─── Construction ─────────────────────────────────────────────────────────────

ZoneServer::ZoneServer(ZoneServerConfig config)
    : m_config(std::move(config))
    , m_workGuard(asio::make_work_guard(m_ioContext))
{
    m_lastStatsLog = std::chrono::steady_clock::now();
}

ZoneServer::~ZoneServer()
{
    Destroy();
}

// ─── Lifecycle ────────────────────────────────────────────────────────────────

bool ZoneServer::Initialize()
{
    assert(!m_running.load(std::memory_order_relaxed) && "Initialize called on a running server");

    LOG_INFO(kTag, "Initializing zone %u (%s) on %s:%u — tick %u Hz, replication %u Hz",
             m_config.zoneId,
             m_config.zoneName.c_str(),
             m_config.bindAddress.c_str(),
             m_config.port,
             m_config.tickRateHz,
             m_config.replicationRateHz);

    if (!InitJobSystem())   return false;
    if (!InitSimulation())  return false;
    if (!InitInterest())    return false;
    if (!InitReplication()) return false;
    if (!InitNetworking())  return false;

    LOG_INFO(kTag, "Zone %u initialised successfully — bound to port %u",
             m_config.zoneId, m_boundPort.load());
    return true;
}

void ZoneServer::Run()
{
    assert(!m_running.load(std::memory_order_relaxed) && "Run called on an already running server");

    m_running.store(true, std::memory_order_release);
    m_shutdownRequested.store(false, std::memory_order_relaxed);

    const uint32_t ioThreadCount = (m_config.ioThreadCount == 0)
        ? std::max(1u, std::thread::hardware_concurrency())
        : m_config.ioThreadCount;

    LOG_INFO(kTag, "Zone %u starting io_context with %u thread(s)", m_config.zoneId, ioThreadCount);

    for (uint32_t i = 0; i < ioThreadCount; ++i)
    {
        m_ioThreads.emplace_back([this, i]()
        {
            LOG_DEBUG(kTag, "IO thread %u started", i);
            try
            {
                m_ioContext.run();
            }
            catch (const std::exception& ex)
            {
                LOG_ERROR(kTag, "IO thread %u threw exception: %s — restarting poll", i, ex.what());
                m_ioContext.run();
            }
            LOG_DEBUG(kTag, "IO thread %u exited", i);
        });
    }

    StartAccept();

    NotifyServerStarted();

    m_tickThread = std::thread([this]() { TickLoop(); });

    if (m_tickThread.joinable())
        m_tickThread.join();

    m_workGuard.reset();
    m_ioContext.stop();

    for (auto& t : m_ioThreads)
        if (t.joinable()) t.join();

    m_ioThreads.clear();

    NotifyServerStopped();

    LOG_INFO(kTag, "Zone %u Run() exited — ticks processed: %llu",
             m_config.zoneId,
             static_cast<unsigned long long>(m_stats.totalTicksProcessed.load()));
}

void ZoneServer::RequestShutdown()
{
    LOG_INFO(kTag, "Zone %u shutdown requested", m_config.zoneId);
    m_shutdownRequested.store(true, std::memory_order_release);
}

void ZoneServer::Destroy()
{
    if (m_running.load(std::memory_order_acquire))
        RequestShutdown();

    if (m_replicationManager)
    {
        m_replicationManager->Shutdown();
        m_replicationManager.reset();
    }

    if (m_connectionManager)
    {
        m_connectionManager->Shutdown();
        m_connectionManager.reset();
    }

    m_interestManager.reset();
    m_worldState.reset();
    m_simTick.reset();

    m_running.store(false, std::memory_order_release);
}

// ─── Init helpers ─────────────────────────────────────────────────────────────

bool ZoneServer::InitJobSystem()
{
    LOG_DEBUG(kTag, "Job system already initialised via engine core — skipping dedicated init");
    return true;
}

bool ZoneServer::InitSimulation()
{
    m_simTick  = std::make_unique<SagaEngine::Simulation::SimulationTick>(m_config.tickRateHz);
    m_worldState = std::make_unique<SagaEngine::Simulation::WorldState>();

    LOG_INFO(kTag, "Simulation initialised — %.4f ms fixed delta",
             (1.0 / static_cast<double>(m_config.tickRateHz)) * 1000.0);
    return true;
}

bool ZoneServer::InitInterest()
{
    using namespace SagaEngine::Networking::Interest;

    InterestConfig cfg;
    cfg.defaultRadius       = 500.0f;
    cfg.updateFrequency     = static_cast<float>(m_config.replicationRateHz);
    cfg.maxEntitiesPerClient = m_config.maxClients * 64;

    m_interestManager = std::make_unique<InterestManager>(cfg);
    LOG_INFO(kTag, "Interest manager initialised — default radius %.1f", cfg.defaultRadius);
    return true;
}

bool ZoneServer::InitReplication()
{
    using namespace SagaEngine::Networking::Replication;

    m_replicationManager = std::make_unique<ReplicationManager>();
    m_replicationManager->Initialize(m_config.maxClients);
    m_replicationManager->SetWorldState(m_worldState.get());
    m_replicationManager->SetInterestManager(m_interestManager.get());
    m_replicationManager->SetReplicationFrequency(
        static_cast<float>(m_config.replicationRateHz));

    LOG_INFO(kTag, "Replication manager initialised — %.1f Hz, max clients %u",
             static_cast<float>(m_config.replicationRateHz),
             m_config.maxClients);
    return true;
}

bool ZoneServer::InitNetworking()
{
    ConnectionManagerConfig connCfg;
    connCfg.maxClients            = m_config.maxClients;
    connCfg.heartbeatIntervalMs   = 1000;
    connCfg.connectionTimeoutMs   = 10000;
    connCfg.receiveBufferSize     = 65536;
    connCfg.sendQueueCapacity     = 4096;

    m_connectionManager = std::make_unique<ConnectionManager>(connCfg, m_ioContext);

    m_connectionManager->SetOnClientConnected([this](ClientId clientId)
    {
        m_replicationManager->AddClient(clientId);
        m_stats.currentClientCount.fetch_add(1, std::memory_order_relaxed);
        NotifyClientConnected(clientId);
        LOG_INFO(kTag, "Client %llu connected — zone %u (%u total)",
                 static_cast<unsigned long long>(clientId),
                 m_config.zoneId,
                 m_stats.currentClientCount.load());
    });

    m_connectionManager->SetOnClientDisconnected([this](ClientId clientId)
    {
        m_replicationManager->RemoveClient(clientId);
        const uint32_t prev = m_stats.currentClientCount.fetch_sub(1, std::memory_order_relaxed);
        if (prev == 0)
            m_stats.currentClientCount.store(0, std::memory_order_relaxed);
        NotifyClientDisconnected(clientId);
        LOG_INFO(kTag, "Client %llu disconnected — zone %u (%u remaining)",
                 static_cast<unsigned long long>(clientId),
                 m_config.zoneId,
                 m_stats.currentClientCount.load());
    });

    m_connectionManager->SetOnPacketReceived([this](ClientId clientId,
                                                    const uint8_t* data,
                                                    std::size_t    size)
    {
        m_stats.totalPacketsReceived.fetch_add(1, std::memory_order_relaxed);
        m_stats.totalBytesReceived.fetch_add(size, std::memory_order_relaxed);
    });

    if (!m_connectionManager->Initialize())
    {
        LOG_ERROR(kTag, "ConnectionManager initialization failed");
        return false;
    }

    asio::error_code ec;
    const asio::ip::address addr = asio::ip::make_address(m_config.bindAddress, ec);
    if (ec)
    {
        LOG_ERROR(kTag, "Invalid bind address '%s': %s",
                  m_config.bindAddress.c_str(), ec.message().c_str());
        return false;
    }

    const asio::ip::tcp::endpoint endpoint(addr, m_config.port);

    m_acceptor = std::make_unique<asio::ip::tcp::acceptor>(m_ioContext);
    m_acceptor->open(endpoint.protocol(), ec);
    if (ec) { LOG_ERROR(kTag, "acceptor open: %s", ec.message().c_str()); return false; }

    m_acceptor->set_option(asio::ip::tcp::acceptor::reuse_address(true), ec);
    m_acceptor->bind(endpoint, ec);
    if (ec) { LOG_ERROR(kTag, "acceptor bind: %s", ec.message().c_str()); return false; }

    m_acceptor->listen(asio::socket_base::max_listen_connections, ec);
    if (ec) { LOG_ERROR(kTag, "acceptor listen: %s", ec.message().c_str()); return false; }

    m_boundPort.store(
        m_acceptor->local_endpoint().port(),
        std::memory_order_release);

    LOG_INFO(kTag, "TCP acceptor listening on %s:%u",
             m_config.bindAddress.c_str(), m_boundPort.load());
    return true;
}

// ─── Accept loop ──────────────────────────────────────────────────────────────

void ZoneServer::StartAccept()
{
    if (!m_acceptor || !m_acceptor->is_open())
        return;

    m_acceptor->async_accept(
        [this](const asio::error_code& ec, asio::ip::tcp::socket socket)
        {
            HandleAccept(ec, std::move(socket));
        });
}

void ZoneServer::HandleAccept(const asio::error_code& ec,
                               asio::ip::tcp::socket   socket)
{
    if (m_shutdownRequested.load(std::memory_order_acquire))
        return;

    if (ec)
    {
        if (ec != asio::error::operation_aborted)
            LOG_WARN(kTag, "Accept error: %s — continuing accept loop", ec.message().c_str());
        StartAccept();
        return;
    }

    const uint32_t current = m_stats.currentClientCount.load(std::memory_order_relaxed);
    if (current >= m_config.maxClients)
    {
        LOG_WARN(kTag, "Client cap reached (%u/%u) — rejecting incoming connection from %s",
                 current, m_config.maxClients,
                 socket.remote_endpoint(const_cast<asio::error_code&>(ec)).address().to_string().c_str());
        asio::error_code closeEc;
        socket.close(closeEc);
        StartAccept();
        return;
    }

    const std::string remoteAddr = [&]() -> std::string
    {
        asio::error_code remoteEc;
        const auto ep = socket.remote_endpoint(remoteEc);
        return remoteEc ? "<unknown>" : ep.address().to_string() + ":" + std::to_string(ep.port());
    }();

    LOG_DEBUG(kTag, "Incoming connection from %s", remoteAddr.c_str());

    asio::error_code nodelayEc;
    socket.set_option(asio::ip::tcp::no_delay(true), nodelayEc);

    m_connectionManager->AcceptSocket(std::move(socket));

    StartAccept();
}

// ─── Tick loop ────────────────────────────────────────────────────────────────

void ZoneServer::TickLoop()
{
    LOG_INFO(kTag, "Tick loop started — %u Hz on zone %u", m_config.tickRateHz, m_config.zoneId);

    m_simTick->Reset();

    while (!m_shutdownRequested.load(std::memory_order_acquire))
    {
        const auto tickStart = std::chrono::steady_clock::now();

        // ── Advance time accumulator ─────────────────────────────────────────

        const auto deadline = m_simTick->NextTickDeadline();
        std::this_thread::sleep_until(deadline);

        const auto wakeTime = std::chrono::steady_clock::now();
        const double wallDelta = std::chrono::duration<double>(wakeTime - tickStart).count();

        const uint32_t ticksToRun = m_simTick->Advance(wallDelta);

        for (uint32_t t = 0; t < ticksToRun; ++t)
        {
            const uint64_t tickIndex = m_simTick->CurrentTick() - (ticksToRun - 1 - t);
            ExecuteTick(tickIndex, m_simTick->FixedDelta());
        }

        // ── Stats & logging ───────────────────────────────────────────────────

        const uint64_t durationUs = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now() - tickStart).count());

        UpdateTickStats(durationUs);

        const auto now = std::chrono::steady_clock::now();
        const auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - m_lastStatsLog).count();

        if (static_cast<uint32_t>(elapsedMs) >= m_config.statsIntervalMs)
        {
            LogPeriodicStats();
            m_lastStatsLog = now;
        }
    }

    LOG_INFO(kTag, "Tick loop exiting — zone %u", m_config.zoneId);
}

void ZoneServer::ExecuteTick(uint64_t tickIndex, double fixedDelta)
{
    const auto tickStart = std::chrono::steady_clock::now();

    // ── Phase 1: drain inbound input packets ─────────────────────────────────
    DrainInputPackets();

    // ── Phase 2: step authoritative simulation ────────────────────────────────
    StepSimulation(tickIndex, fixedDelta);

    // ── Phase 3: flush replication to all connected clients ──────────────────
    FlushReplication(tickIndex);

    // ── Phase 4: pump pending Asio I/O without blocking ─────────────────────
    PumpNetworkIO();

    // ── Phase 5: tick accounting ──────────────────────────────────────────────
    m_stats.totalTicksProcessed.fetch_add(1, std::memory_order_relaxed);

    const uint64_t durationUs = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - tickStart).count());

    m_stats.lastTickDurationUs.store(durationUs, std::memory_order_relaxed);

    if (durationUs > m_config.maxTickBudgetUs)
    {
        m_stats.totalTickBudgetOverruns.fetch_add(1, std::memory_order_relaxed);
        NotifyTickCompleted(tickIndex, durationUs);
        LOG_WARN(kTag, "Tick %llu budget overrun: %llu µs (budget %u µs)",
                 static_cast<unsigned long long>(tickIndex),
                 static_cast<unsigned long long>(durationUs),
                 m_config.maxTickBudgetUs);
    }
    else
    {
        NotifyTickCompleted(tickIndex, durationUs);
    }
}

void ZoneServer::DrainInputPackets()
{
    if (!m_connectionManager)
        return;

    m_connectionManager->DrainInboundPackets(
        [this](ClientId clientId, const uint8_t* data, std::size_t size)
        {
            (void)clientId; (void)data; (void)size;
        });
}

void ZoneServer::StepSimulation(uint64_t /*tickIndex*/, double /*fixedDelta*/)
{
    if (!m_worldState)
        return;

    // TODO: Integrate SubsystemManager or Authority::Tick to drive
    // physics, movement authority checks, and ECS system updates.
    // WorldState is currently a data-only container and does not support
    // simulation stepping. This is a placeholder until the proper
    // simulation orchestration layer is wired in.
    (void)m_worldState; // Suppress unused warning
}

void ZoneServer::FlushReplication(uint64_t /*tickIndex*/)
{
    if (!m_replicationManager || !m_connectionManager)
        return;

    const float dt = static_cast<float>(m_simTick->FixedDelta());
    m_replicationManager->GatherDirtyEntities(dt);

    m_connectionManager->ForEachConnectedClient(
        [this](ClientId clientId, ConnectionSendFn sendFn)
        {
            m_replicationManager->SendUpdates(clientId,
                [this, &sendFn](const uint8_t* data, std::size_t size)
                {
                    sendFn(data, size);
                    m_stats.totalPacketsSent.fetch_add(1, std::memory_order_relaxed);
                    m_stats.totalBytesSent.fetch_add(size, std::memory_order_relaxed);
                });
        });
}

void ZoneServer::PumpNetworkIO()
{
    // poll() rather than run() — returns immediately when no handlers are ready.
    // This lets the tick thread touch Asio handlers (timers, completions) without
    // blocking on the next I/O event.
    m_ioContext.poll();
}

// ─── Stats ────────────────────────────────────────────────────────────────────

void ZoneServer::UpdateTickStats(uint64_t durationUs)
{
    m_stats.lastTickDurationUs.store(durationUs, std::memory_order_relaxed);

    if (durationUs > m_stats.peakTickDurationUs.load(std::memory_order_relaxed))
        m_stats.peakTickDurationUs.store(durationUs, std::memory_order_relaxed);

    m_tickDurationAccumUs += durationUs;
    ++m_tickDurationSampleCount;

    if (m_tickDurationSampleCount >= kTickDurationWindowSize)
    {
        m_stats.averageTickDurationUs.store(
            m_tickDurationAccumUs / m_tickDurationSampleCount,
            std::memory_order_relaxed);
        m_tickDurationAccumUs      = 0;
        m_tickDurationSampleCount  = 0;
    }
}

void ZoneServer::LogPeriodicStats()
{
    LOG_INFO(kTag,
        "Zone %u stats — tick: %llu | clients: %u | pkts tx: %llu | pkts rx: %llu "
        "| bytes tx: %llu | bytes rx: %llu | avg tick: %llu µs | peak tick: %llu µs "
        "| budget overruns: %llu",
        m_config.zoneId,
        static_cast<unsigned long long>(m_stats.totalTicksProcessed.load()),
        m_stats.currentClientCount.load(),
        static_cast<unsigned long long>(m_stats.totalPacketsSent.load()),
        static_cast<unsigned long long>(m_stats.totalPacketsReceived.load()),
        static_cast<unsigned long long>(m_stats.totalBytesSent.load()),
        static_cast<unsigned long long>(m_stats.totalBytesReceived.load()),
        static_cast<unsigned long long>(m_stats.averageTickDurationUs.load()),
        static_cast<unsigned long long>(m_stats.peakTickDurationUs.load()),
        static_cast<unsigned long long>(m_stats.totalTickBudgetOverruns.load()));
}

// ─── Queries ──────────────────────────────────────────────────────────────────

bool ZoneServer::IsRunning() const noexcept
{
    return m_running.load(std::memory_order_acquire);
}

uint32_t ZoneServer::GetZoneId() const noexcept
{
    return m_config.zoneId;
}

uint16_t ZoneServer::GetBoundPort() const noexcept
{
    return m_boundPort.load(std::memory_order_relaxed);
}

uint32_t ZoneServer::GetCurrentClientCount() const noexcept
{
    return m_stats.currentClientCount.load(std::memory_order_relaxed);
}

const ZoneServerStats& ZoneServer::GetStats() const noexcept
{
    return m_stats;
}

const ZoneServerConfig& ZoneServer::GetConfig() const noexcept
{
    return m_config;
}

SagaEngine::Simulation::WorldState* ZoneServer::GetWorldState() noexcept
{
    return m_worldState.get();
}

SagaEngine::Networking::Replication::ReplicationManager*
ZoneServer::GetReplicationManager() noexcept
{
    return m_replicationManager.get();
}

// ─── Observer ─────────────────────────────────────────────────────────────────

void ZoneServer::AddListener(IZoneServerListener* listener)
{
    std::lock_guard<std::mutex> lock(m_listenerMutex);
    m_listeners.push_back(listener);
}

void ZoneServer::RemoveListener(IZoneServerListener* listener)
{
    std::lock_guard<std::mutex> lock(m_listenerMutex);
    m_listeners.erase(
        std::remove(m_listeners.begin(), m_listeners.end(), listener),
        m_listeners.end());
}

void ZoneServer::NotifyServerStarted()
{
    std::lock_guard<std::mutex> lock(m_listenerMutex);
    for (auto* l : m_listeners) l->OnServerStarted(m_config.zoneId, m_boundPort.load());
}

void ZoneServer::NotifyServerStopped()
{
    std::lock_guard<std::mutex> lock(m_listenerMutex);
    for (auto* l : m_listeners) l->OnServerStopped(m_config.zoneId);
}

void ZoneServer::NotifyClientConnected(ClientId clientId)
{
    std::lock_guard<std::mutex> lock(m_listenerMutex);
    for (auto* l : m_listeners) l->OnClientConnected(clientId, m_config.zoneId);
}

void ZoneServer::NotifyClientDisconnected(ClientId clientId)
{
    std::lock_guard<std::mutex> lock(m_listenerMutex);
    for (auto* l : m_listeners) l->OnClientDisconnected(clientId, m_config.zoneId);
}

void ZoneServer::NotifyTickCompleted(uint64_t tick, uint64_t durationUs)
{
    std::lock_guard<std::mutex> lock(m_listenerMutex);
    for (auto* l : m_listeners) l->OnTickCompleted(tick, durationUs);
}

// ─── Tick injection (tests) ───────────────────────────────────────────────────

void ZoneServer::ForceTick(double wallDeltaSeconds)
{
    assert(!m_running.load(std::memory_order_relaxed) &&
           "ForceTick must not be called while Run() is active");

    const uint32_t ticks = m_simTick->Advance(wallDeltaSeconds);
    for (uint32_t t = 0; t < ticks; ++t)
        ExecuteTick(m_simTick->CurrentTick() - (ticks - 1 - t),
                    m_simTick->FixedDelta());
}

} // namespace SagaServer::Networking
