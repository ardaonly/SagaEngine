/// @file ZoneServer.cpp
/// @brief ZoneServer — local-only authoritative simulation harness.

#include "SagaEngine/ServerAuthority/ZoneServer.h"
#include "SagaEngine/ServerAuthority/ConnectionManager.h"
#include "SagaEngine/Networking/NetworkTypes.h"
#include "SagaEngine/Simulation/SimulationTick.h"
#include "SagaEngine/Simulation/WorldState.h"
#include "SagaEngine/Core/Log/Log.h"
#include "SagaEngine/Diagnostics/DiagnosticSystem.hpp"

#include <asio.hpp>
#include <asio/steady_timer.hpp>

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cmath>
#include <thread>

namespace SagaEngine::ServerAuthority
{

using namespace std::chrono_literals;

static constexpr const char* kTag = "ZoneServer";

struct ZoneServer::NetworkRuntime
{
    explicit NetworkRuntime(ZoneServer& server)
        : owner(server)
        , workGuard(asio::make_work_guard(ioContext))
    {
    }

    void StartAccept();
    void HandleAccept(const asio::error_code& ec, asio::ip::tcp::socket socket);

    ZoneServer& owner;
    asio::io_context ioContext;
    asio::executor_work_guard<asio::io_context::executor_type> workGuard;
    std::unique_ptr<asio::ip::tcp::acceptor> acceptor;
    std::vector<std::thread> ioThreads;
    std::unique_ptr<ConnectionManager> connectionManager;
};

// ─── Construction ─────────────────────────────────────────────────────────────

ZoneServer::ZoneServer(ZoneServerConfig config)
    : m_config(std::move(config))
    , m_networkRuntime(std::make_unique<NetworkRuntime>(*this))
    , m_actorOwnershipRegistry(
          std::make_unique<SagaEngine::ServerAuthority::Simulation::ActorOwnershipRegistry>())
    , m_movementCommandIntake(
          std::make_unique<SagaEngine::ServerAuthority::Simulation::AuthoritativeMovementCommandIntake>())
    , m_movementDirtyReplicationBridge(
          std::make_unique<SagaEngine::Replication::MovementDirtyReplicationBridge>())
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

    asio::error_code addressError;
    const auto bindAddress = asio::ip::make_address(m_config.bindAddress, addressError);
    if (addressError || !bindAddress.is_loopback())
    {
        LOG_ERROR(kTag,
                  "Refusing non-loopback bind address '%s'; hosted networking is not supported",
                  m_config.bindAddress.c_str());
        return false;
    }

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
        m_networkRuntime->ioThreads.emplace_back([this, i]()
        {
            LOG_DEBUG(kTag, "IO thread %u started", i);
            try
            {
                m_networkRuntime->ioContext.run();
            }
            catch (const std::exception& ex)
            {
                LOG_ERROR(kTag, "IO thread %u threw exception: %s — restarting poll", i, ex.what());
                m_networkRuntime->ioContext.run();
            }
            LOG_DEBUG(kTag, "IO thread %u exited", i);
        });
    }

    m_networkRuntime->StartAccept();

    NotifyServerStarted();

    m_tickThread = std::thread([this]() { TickLoop(); });

    if (m_tickThread.joinable())
        m_tickThread.join();

    m_networkRuntime->workGuard.reset();
    m_networkRuntime->ioContext.stop();

    for (auto& t : m_networkRuntime->ioThreads)
        if (t.joinable()) t.join();

    m_networkRuntime->ioThreads.clear();

    NotifyServerStopped();

    LOG_INFO(kTag, "Zone %u Run() exited — ticks processed: %llu",
             m_config.zoneId,
             static_cast<unsigned long long>(m_stats.totalTicksProcessed.load()));
}

void ZoneServer::RequestShutdown()
{
    LOG_INFO(kTag, "Zone %u shutdown requested", m_config.zoneId);
    const bool wasRequested = m_shutdownRequested.exchange(
        true,
        std::memory_order_release);
    if (!wasRequested)
    {
        RecordShutdownRequestedLifecycle();
    }
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

    if (m_networkRuntime->connectionManager)
    {
        m_networkRuntime->connectionManager->Shutdown();
        m_networkRuntime->connectionManager.reset();
    }

    m_interestManager.reset();
    m_worldState.reset();
    m_simTick.reset();

    m_running.store(false, std::memory_order_release);
}

void ZoneServer::SetDiagnostics(
    SagaEngine::Diagnostics::DiagnosticSystem* diagnostics) noexcept
{
    m_diagnostics = diagnostics;
    if (!m_diagnostics)
        return;

    m_diagnostics->Log().Log(
        SagaEngine::Core::Log::Level::Info,
        kTag,
        "Diagnostics attached to zone server",
        {{"zone_id", std::to_string(m_config.zoneId)},
         {"zone_name", m_config.zoneName}});
    RecordActiveControlledActorCount();
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
    using namespace SagaEngine::Replication::Interest;

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
    using namespace SagaEngine::Replication;

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

    m_networkRuntime->connectionManager =
        std::make_unique<ConnectionManager>(
            connCfg, m_networkRuntime->ioContext);

    m_networkRuntime->connectionManager->SetOnClientConnected([this](ClientId clientId)
    {
        m_replicationManager->AddClient(clientId);
        m_stats.currentClientCount.fetch_add(1, std::memory_order_relaxed);
        RecordSessionConnectedLifecycle(clientId);
        NotifyClientConnected(clientId);
        LOG_INFO(kTag, "Client %llu connected — zone %u (%u total)",
                 static_cast<unsigned long long>(clientId),
                 m_config.zoneId,
                 m_stats.currentClientCount.load());
    });

    m_networkRuntime->connectionManager->SetOnClientDisconnected([this](ClientId clientId)
    {
        m_replicationManager->RemoveClient(clientId);
        (void)UnregisterControlledActor(clientId);
        const uint32_t prev = m_stats.currentClientCount.fetch_sub(1, std::memory_order_relaxed);
        if (prev == 0)
            m_stats.currentClientCount.store(0, std::memory_order_relaxed);
        RecordSessionDisconnectedLifecycle(clientId);
        NotifyClientDisconnected(clientId);
        LOG_INFO(kTag, "Client %llu disconnected — zone %u (%u remaining)",
                 static_cast<unsigned long long>(clientId),
                 m_config.zoneId,
                 m_stats.currentClientCount.load());
    });

    m_networkRuntime->connectionManager->SetOnPacketReceived([this](ClientId clientId,
                                                    const uint8_t* data,
                                                    std::size_t    size)
    {
        m_stats.totalPacketsReceived.fetch_add(1, std::memory_order_relaxed);
        m_stats.totalBytesReceived.fetch_add(size, std::memory_order_relaxed);
    });

    if (!m_networkRuntime->connectionManager->Initialize())
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

    m_networkRuntime->acceptor = std::make_unique<asio::ip::tcp::acceptor>(m_networkRuntime->ioContext);
    m_networkRuntime->acceptor->open(endpoint.protocol(), ec);
    if (ec) { LOG_ERROR(kTag, "acceptor open: %s", ec.message().c_str()); return false; }

    m_networkRuntime->acceptor->set_option(asio::ip::tcp::acceptor::reuse_address(true), ec);
    m_networkRuntime->acceptor->bind(endpoint, ec);
    if (ec) { LOG_ERROR(kTag, "acceptor bind: %s", ec.message().c_str()); return false; }

    m_networkRuntime->acceptor->listen(asio::socket_base::max_listen_connections, ec);
    if (ec) { LOG_ERROR(kTag, "acceptor listen: %s", ec.message().c_str()); return false; }

    m_boundPort.store(
        m_networkRuntime->acceptor->local_endpoint().port(),
        std::memory_order_release);

    LOG_INFO(kTag, "TCP acceptor listening on %s:%u",
             m_config.bindAddress.c_str(), m_boundPort.load());
    return true;
}

// ─── Accept loop ──────────────────────────────────────────────────────────────

void ZoneServer::NetworkRuntime::StartAccept()
{
    if (!acceptor || !acceptor->is_open())
        return;

    acceptor->async_accept(
        [this](const asio::error_code& ec, asio::ip::tcp::socket socket)
        {
            HandleAccept(ec, std::move(socket));
        });
}

void ZoneServer::NetworkRuntime::HandleAccept(
    const asio::error_code& ec, asio::ip::tcp::socket socket)
{
    if (owner.m_shutdownRequested.load(std::memory_order_acquire))
        return;

    if (ec)
    {
        if (ec != asio::error::operation_aborted)
            LOG_WARN(kTag, "Accept error: %s — continuing accept loop", ec.message().c_str());
        StartAccept();
        return;
    }

    const uint32_t current = owner.m_stats.currentClientCount.load(std::memory_order_relaxed);
    if (current >= owner.m_config.maxClients)
    {
        LOG_WARN(kTag, "Client cap reached (%u/%u) — rejecting incoming connection from %s",
                 current, owner.m_config.maxClients,
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

    connectionManager->AcceptSocket(std::move(socket));

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

    // ── Step 1: drain inbound input packets ─────────────────────────────────
    DrainInputPackets();

    // ── Step 2: step authoritative simulation ────────────────────────────────
    StepSimulation(tickIndex, fixedDelta);

    // ── Step 3: flush replication to all connected clients ──────────────────
    FlushReplication(tickIndex);

    // ── Step 4: pump pending Asio I/O without blocking ─────────────────────
    PumpNetworkIO();

    // ── Step 5: tick accounting ──────────────────────────────────────────────
    m_stats.totalTicksProcessed.fetch_add(1, std::memory_order_relaxed);

    const uint64_t durationUs = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - tickStart).count());

    m_stats.lastTickDurationUs.store(durationUs, std::memory_order_relaxed);
    RecordTickReliabilityDiagnostics(tickIndex, durationUs);

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
    if (!m_networkRuntime->connectionManager)
        return;

    m_networkRuntime->connectionManager->DrainInboundPackets(
        [this](ClientId clientId, const uint8_t* data, std::size_t size)
        {
            (void)ProcessRawInboundPacket(clientId, data, size);
        });
}

::SagaEngine::Networking::ServerPacketNormalizationStatus
ZoneServer::ProcessRawInboundPacket(
    ClientId clientId, const std::uint8_t* data, std::size_t size)
{
    const auto result = ::SagaEngine::Networking::NormalizeServerPacketFrame(
        clientId, data, size);
    if (!result.Succeeded())
    {
        m_stats.totalPacketsRejected.fetch_add(1, std::memory_order_relaxed);
        RecordDiagnosticCounter("server.packets.rejected");
        if (m_config.enableDetailedPacketLog)
        {
            LOG_WARN(kTag,
                     "Rejected inbound frame from client %llu: status=%u, bytes=%zu",
                     static_cast<unsigned long long>(clientId),
                     static_cast<unsigned>(result.status),
                     size);
        }
        return result.status;
    }

    m_stats.totalPacketsNormalized.fetch_add(1, std::memory_order_relaxed);
    HandleNormalizedInboundPacket(result.packet);
    return result.status;
}

void ZoneServer::HandleNormalizedInboundPacket(
    const ::SagaEngine::Networking::NormalizedServerPacketView& packet)
{
    if (m_config.enableDetailedPacketLog)
    {
        LOG_DEBUG(kTag,
                  "Normalized inbound packet from client %llu: type=%u, payload=%zu",
                  static_cast<unsigned long long>(packet.clientId),
                  static_cast<unsigned>(packet.packetType),
                  packet.payloadSize);
    }

    NotifyInboundPacketNormalized(packet);

    if (packet.packetType != SagaEngine::Networking::PacketType::InputCommand ||
        !m_movementCommandIntake)
    {
        return;
    }

    SagaEngine::ServerAuthority::Simulation::AuthoritativeMovementCommandPacketView intakePacket;
    intakePacket.clientId = packet.clientId;
    intakePacket.packetType = packet.packetType;
    intakePacket.payload = packet.payload;
    intakePacket.payloadSize = packet.payloadSize;
    intakePacket.serverTick = m_simTick ? m_simTick->CurrentTick() : 0;
    intakePacket.recvTimeUnixMs = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());

    const auto intakeResult = m_movementCommandIntake->HandlePacket(intakePacket);
    RecordMovementIntakeDiagnostics(intakeResult);
    {
        std::lock_guard<std::mutex> lock(m_movementAuthorityMutex);
        m_lastMovementIntakeResult = intakeResult;
    }
}

void ZoneServer::StepSimulation(uint64_t tickIndex, double fixedDelta)
{
    (void)TickMovementAuthority(tickIndex, fixedDelta);

    if (!m_worldState)
        return;

    // TODO: Integrate SubsystemManager or Authority::Tick to drive
    // physics, movement authority checks, and ECS system updates.
    // WorldState is currently a data-only container and does not support
    // simulation stepping. This is a placeholder until the proper
    // simulation orchestration layer is wired in.
    (void)m_worldState; // Suppress unused warning
}

SagaEngine::ServerAuthority::Simulation::AuthoritativeMovementTickReport
ZoneServer::TickMovementAuthority(uint64_t tickIndex, double fixedDelta)
{
    SagaEngine::ServerAuthority::Simulation::AuthoritativeMovementTickReport report;
    RecordDiagnosticCounter("server.tick.count");
    if (!m_movementCommandIntake || fixedDelta <= 0.0 || !std::isfinite(fixedDelta))
    {
        std::lock_guard<std::mutex> lock(m_movementAuthorityMutex);
        m_lastMovementTickReport = report;
        return report;
    }

    report = m_movementCommandIntake->Tick(
        tickIndex,
        static_cast<float>(fixedDelta));

    if (m_movementDirtyReplicationBridge)
        m_movementDirtyReplicationBridge->RecordMovementTick(report, tickIndex);

    {
        std::lock_guard<std::mutex> lock(m_movementAuthorityMutex);
        m_lastMovementTickReport = report;
    }

    return report;
}

void ZoneServer::FlushReplication(uint64_t /*tickIndex*/)
{
    if (!m_replicationManager || !m_networkRuntime->connectionManager)
        return;

    const float dt = static_cast<float>(m_simTick->FixedDelta());
    m_replicationManager->GatherDirtyEntities(dt);

    m_networkRuntime->connectionManager->ForEachConnectedClient(
        [this](ClientId clientId,
               ConnectionSendFn sendFn)
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
    m_networkRuntime->ioContext.poll();
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
        "| pkts normalized: %llu | pkts rejected: %llu | bytes tx: %llu | bytes rx: %llu "
        "| avg tick: %llu µs | peak tick: %llu µs | budget overruns: %llu",
        m_config.zoneId,
        static_cast<unsigned long long>(m_stats.totalTicksProcessed.load()),
        m_stats.currentClientCount.load(),
        static_cast<unsigned long long>(m_stats.totalPacketsSent.load()),
        static_cast<unsigned long long>(m_stats.totalPacketsReceived.load()),
        static_cast<unsigned long long>(m_stats.totalPacketsNormalized.load()),
        static_cast<unsigned long long>(m_stats.totalPacketsRejected.load()),
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

SagaEngine::Replication::ReplicationManager*
ZoneServer::GetReplicationManager() noexcept
{
    return m_replicationManager.get();
}

SagaEngine::ServerAuthority::Simulation::ActorOwnershipResult
ZoneServer::RegisterControlledActor(
    ClientId clientId,
    SagaEngine::ServerAuthority::Simulation::EntityId entityId,
    SagaEngine::ServerAuthority::Simulation::Vector3 initialPosition)
{
    if (!m_actorOwnershipRegistry || !m_movementCommandIntake)
        return SagaEngine::ServerAuthority::Simulation::ActorOwnershipResult::NotFound;

    const auto result = m_actorOwnershipRegistry->RegisterOwnership(
        clientId,
        entityId,
        initialPosition);

    if (result != SagaEngine::ServerAuthority::Simulation::ActorOwnershipResult::Registered)
        return result;

    if (!m_movementCommandIntake->RegisterActor(clientId, entityId, initialPosition))
    {
        (void)m_actorOwnershipRegistry->UnregisterClient(clientId);
        return SagaEngine::ServerAuthority::Simulation::ActorOwnershipResult::NotFound;
    }

    RecordActiveControlledActorCount();
    RecordEntityCreatedLifecycle(clientId, entityId);
    return result;
}

SagaEngine::ServerAuthority::Simulation::ActorOwnershipResult
ZoneServer::UnregisterControlledActor(ClientId clientId)
{
    if (!m_actorOwnershipRegistry || !m_movementCommandIntake)
        return SagaEngine::ServerAuthority::Simulation::ActorOwnershipResult::NotFound;

    const auto existingActor = m_actorOwnershipRegistry->FindByClient(clientId);
    const auto result = m_actorOwnershipRegistry->UnregisterClient(clientId);
    if (result == SagaEngine::ServerAuthority::Simulation::ActorOwnershipResult::Unregistered)
    {
        m_movementCommandIntake->UnregisterActor(clientId);
        RecordActiveControlledActorCount();
        if (existingActor.has_value())
        {
            RecordEntityDestroyedLifecycle(clientId, existingActor->entityId);
        }
    }

    return result;
}

std::optional<SagaEngine::ServerAuthority::Simulation::ControlledActor>
ZoneServer::FindControlledActor(ClientId clientId) const
{
    if (!m_actorOwnershipRegistry)
        return std::nullopt;

    return m_actorOwnershipRegistry->FindByClient(clientId);
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
    RecordServerStartedLifecycle();
    std::lock_guard<std::mutex> lock(m_listenerMutex);
    for (auto* l : m_listeners) l->OnServerStarted(m_config.zoneId, m_boundPort.load());
}

void ZoneServer::NotifyServerStopped()
{
    RecordServerStoppedLifecycle();
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

void ZoneServer::NotifyInboundPacketNormalized(
    const ::SagaEngine::Networking::NormalizedServerPacketView& packet)
{
    std::lock_guard<std::mutex> lock(m_listenerMutex);
    for (auto* l : m_listeners) l->OnInboundPacketNormalized(packet);
}

void ZoneServer::NotifyTickCompleted(uint64_t tick, uint64_t durationUs)
{
    std::lock_guard<std::mutex> lock(m_listenerMutex);
    for (auto* l : m_listeners) l->OnTickCompleted(tick, durationUs);
}

void ZoneServer::RecordDiagnosticCounter(const char* name, double amount)
{
    if (!m_diagnostics)
        return;

    m_diagnostics->Health().IncrementCounter(name, amount);
}

void ZoneServer::RecordDiagnosticGauge(const char* name, double value)
{
    if (!m_diagnostics)
        return;

    m_diagnostics->Health().SetGauge(name, value);
}

void ZoneServer::RecordDiagnosticTiming(const char* name, double milliseconds)
{
    if (!m_diagnostics)
        return;

    m_diagnostics->Health().RecordTiming(name, milliseconds);
}

void ZoneServer::RecordActiveControlledActorCount()
{
    if (!m_actorOwnershipRegistry)
        return;

    RecordDiagnosticGauge(
        "world.entities.active",
        static_cast<double>(m_actorOwnershipRegistry->Count()));
}

void ZoneServer::RecordMovementIntakeDiagnostics(
    const SagaEngine::ServerAuthority::Simulation::AuthoritativeMovementCommandIntakeResult& result)
{
    using SagaEngine::ServerAuthority::Simulation::AuthoritativeMovementCommandIntakeDecision;
    using SagaEngine::ServerAuthority::Simulation::AuthoritativeMovementDecision;

    if (result.decision ==
        AuthoritativeMovementCommandIntakeDecision::MalformedInput)
    {
        RecordDiagnosticCounter("server.movement.rejected_inputs");
        return;
    }

    if (result.decision !=
        AuthoritativeMovementCommandIntakeDecision::Forwarded)
    {
        return;
    }

    if (result.movementResult.has_value() &&
        result.movementResult->decision == AuthoritativeMovementDecision::Queued)
    {
        RecordDiagnosticCounter("server.movement.accepted_inputs");
        return;
    }

    RecordDiagnosticCounter("server.movement.rejected_inputs");
}

void ZoneServer::RecordTickReliabilityDiagnostics(
    uint64_t tickIndex,
    uint64_t durationUs)
{
    if (!m_diagnostics)
        return;

    RecordDiagnosticTiming(
        "server.tick.ms",
        static_cast<double>(durationUs) / 1000.0);

    if (durationUs <= m_config.maxTickBudgetUs)
        return;

    RecordDiagnosticCounter("server.tick.budget_overruns");
    RecordLifecycleEvent(
        "TickSlow",
        "tick",
        "warning",
        "Server tick exceeded configured budget",
        tickIndex,
        {{"budget_us", std::to_string(m_config.maxTickBudgetUs)},
         {"duration_us", std::to_string(durationUs)}});
    m_diagnostics->Log().Log(
        SagaEngine::Core::Log::Level::Warn,
        kTag,
        "Tick budget overrun",
        {{"zone_id", std::to_string(m_config.zoneId)},
         {"tick", std::to_string(tickIndex)},
         {"duration_us", std::to_string(durationUs)},
         {"budget_us", std::to_string(m_config.maxTickBudgetUs)}});
}

void ZoneServer::RecordLifecycleEvent(
    std::string eventName,
    std::string category,
    std::string severity,
    std::string message,
    uint64_t tick,
    std::vector<std::pair<std::string, std::string>> payload)
{
    if (!m_diagnostics)
        return;

    (void)m_diagnostics->ServerLifecycle().RecordEvent(
        std::move(eventName),
        std::move(category),
        std::move(severity),
        std::move(message),
        m_config.zoneId,
        tick,
        std::move(payload));
}

void ZoneServer::RecordServerStartedLifecycle(uint64_t tick)
{
    if (!m_diagnostics)
        return;

    uint64_t recordId = 0;
    {
        std::lock_guard<std::mutex> lock(m_lifecycleRecordMutex);
        recordId = m_serverLifecycleRecordId;
    }

    if (recordId == 0)
    {
        recordId = m_diagnostics->ServerLifecycle().CreateRecord(
            "Server",
            m_config.zoneId,
            m_config.zoneId,
            0,
            tick,
            "running",
            m_config.zoneName);
        if (recordId != 0)
        {
            std::lock_guard<std::mutex> lock(m_lifecycleRecordMutex);
            m_serverLifecycleRecordId = recordId;
        }
    }

    RecordLifecycleEvent(
        "ServerStarted",
        "server",
        "info",
        "Zone server started",
        tick,
        {{"bound_port", std::to_string(m_boundPort.load())},
         {"record_id", std::to_string(recordId)},
         {"zone_name", m_config.zoneName}});
}

void ZoneServer::RecordServerStoppedLifecycle(uint64_t tick)
{
    if (!m_diagnostics)
        return;

    uint64_t recordId = 0;
    {
        std::lock_guard<std::mutex> lock(m_lifecycleRecordMutex);
        recordId = m_serverLifecycleRecordId;
        m_serverLifecycleRecordId = 0;
    }

    if (recordId != 0)
    {
        (void)m_diagnostics->ServerLifecycle().DestroyRecord(
            recordId,
            tick,
            "stopped");
    }

    RecordLifecycleEvent(
        "ServerStopped",
        "server",
        "info",
        "Zone server stopped",
        tick,
        {{"record_id", std::to_string(recordId)}});
}

void ZoneServer::RecordShutdownRequestedLifecycle(uint64_t tick)
{
    if (!m_diagnostics)
        return;

    RecordLifecycleEvent(
        "ServerShutdownRequested",
        "server",
        "info",
        "Zone server shutdown requested",
        tick,
        {{"zone_name", m_config.zoneName}});
    (void)m_diagnostics->ServerLifecycle().EmitLifetimeLeakEventsForActiveRecords(
        m_config.zoneId,
        tick);
}

void ZoneServer::RecordSessionConnectedLifecycle(ClientId clientId, uint64_t tick)
{
    if (!m_diagnostics)
        return;

    uint64_t recordId = 0;
    {
        std::lock_guard<std::mutex> lock(m_lifecycleRecordMutex);
        const auto it = m_sessionLifecycleRecordIds.find(clientId);
        if (it != m_sessionLifecycleRecordIds.end())
        {
            recordId = it->second;
        }
    }

    if (recordId == 0)
    {
        recordId = m_diagnostics->ServerLifecycle().CreateRecord(
            "SessionConnection",
            m_config.zoneId,
            clientId,
            FindSessionLifecycleRecordId(0),
            tick,
            "connected",
            "client:" + std::to_string(clientId));
        if (recordId != 0)
        {
            std::lock_guard<std::mutex> lock(m_lifecycleRecordMutex);
            m_sessionLifecycleRecordIds[clientId] = recordId;
        }
    }

    RecordLifecycleEvent(
        "SessionConnected",
        "session",
        "info",
        "Client session connected",
        tick,
        {{"client_id", std::to_string(clientId)},
         {"record_id", std::to_string(recordId)}});
}

void ZoneServer::RecordSessionDisconnectedLifecycle(ClientId clientId, uint64_t tick)
{
    if (!m_diagnostics)
        return;

    uint64_t recordId = 0;
    {
        std::lock_guard<std::mutex> lock(m_lifecycleRecordMutex);
        const auto it = m_sessionLifecycleRecordIds.find(clientId);
        if (it != m_sessionLifecycleRecordIds.end())
        {
            recordId = it->second;
            m_sessionLifecycleRecordIds.erase(it);
        }
    }

    if (recordId != 0)
    {
        (void)m_diagnostics->ServerLifecycle().DestroyRecord(
            recordId,
            tick,
            "disconnected");
    }
    else
    {
        (void)m_diagnostics->ServerLifecycle().DestroyActiveRecordByExternalId(
            "SessionConnection",
            m_config.zoneId,
            clientId,
            tick,
            "disconnected");
    }

    RecordLifecycleEvent(
        "SessionDisconnected",
        "session",
        "info",
        "Client session disconnected",
        tick,
        {{"client_id", std::to_string(clientId)},
         {"record_id", std::to_string(recordId)}});
}

void ZoneServer::RecordEntityCreatedLifecycle(
    ClientId clientId,
    SagaEngine::ServerAuthority::Simulation::EntityId entityId,
    uint64_t tick)
{
    if (!m_diagnostics)
        return;

    const uint64_t ownerRecordId = FindSessionLifecycleRecordId(clientId);
    const uint64_t recordId = m_diagnostics->ServerLifecycle().CreateRecord(
        "Entity",
        m_config.zoneId,
        entityId,
        ownerRecordId,
        tick,
        "active",
        "entity:" + std::to_string(entityId));
    if (recordId != 0)
    {
        std::lock_guard<std::mutex> lock(m_lifecycleRecordMutex);
        m_entityLifecycleRecordIdsByClient[clientId] = recordId;
    }

    RecordLifecycleEvent(
        "EntityCreated",
        "entity",
        "info",
        "Controlled entity created",
        tick,
        {{"client_id", std::to_string(clientId)},
         {"entity_id", std::to_string(entityId)},
         {"owner_record_id", std::to_string(ownerRecordId)},
         {"record_id", std::to_string(recordId)}});
}

void ZoneServer::RecordEntityDestroyedLifecycle(
    ClientId clientId,
    SagaEngine::ServerAuthority::Simulation::EntityId entityId,
    uint64_t tick)
{
    if (!m_diagnostics)
        return;

    uint64_t recordId = 0;
    {
        std::lock_guard<std::mutex> lock(m_lifecycleRecordMutex);
        const auto it = m_entityLifecycleRecordIdsByClient.find(clientId);
        if (it != m_entityLifecycleRecordIdsByClient.end())
        {
            recordId = it->second;
            m_entityLifecycleRecordIdsByClient.erase(it);
        }
    }

    if (recordId != 0)
    {
        (void)m_diagnostics->ServerLifecycle().DestroyRecord(
            recordId,
            tick,
            "destroyed");
    }
    else
    {
        (void)m_diagnostics->ServerLifecycle().DestroyActiveRecordByExternalId(
            "Entity",
            m_config.zoneId,
            entityId,
            tick,
            "destroyed");
    }

    RecordLifecycleEvent(
        "EntityDestroyed",
        "entity",
        "info",
        "Controlled entity destroyed",
        tick,
        {{"client_id", std::to_string(clientId)},
         {"entity_id", std::to_string(entityId)},
         {"record_id", std::to_string(recordId)}});
}

uint64_t ZoneServer::FindSessionLifecycleRecordId(ClientId clientId) const
{
    if (clientId == 0)
    {
        std::lock_guard<std::mutex> lock(m_lifecycleRecordMutex);
        return m_serverLifecycleRecordId;
    }

    std::lock_guard<std::mutex> lock(m_lifecycleRecordMutex);
    const auto it = m_sessionLifecycleRecordIds.find(clientId);
    return it == m_sessionLifecycleRecordIds.end() ? 0 : it->second;
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

::SagaEngine::Networking::ServerPacketNormalizationStatus
ZoneServer::ProcessRawInboundPacketForTesting(
    ClientId clientId, const std::uint8_t* data, std::size_t size)
{
    return ProcessRawInboundPacket(clientId, data, size);
}

SagaEngine::ServerAuthority::Simulation::AuthoritativeMovementTickReport
ZoneServer::TickMovementForTesting(uint64_t tickIndex, double fixedDelta)
{
    return TickMovementAuthority(tickIndex, fixedDelta);
}

std::optional<SagaEngine::ServerAuthority::Simulation::Vector3>
ZoneServer::GetControlledActorPositionForTesting(
    SagaEngine::ServerAuthority::Simulation::EntityId entityId) const
{
    if (!m_movementCommandIntake)
        return std::nullopt;

    return m_movementCommandIntake->GetActorPosition(entityId);
}

std::optional<
    SagaEngine::ServerAuthority::Simulation::AuthoritativeMovementCommandIntakeResult>
ZoneServer::GetLastMovementIntakeResultForTesting() const
{
    std::lock_guard<std::mutex> lock(m_movementAuthorityMutex);
    return m_lastMovementIntakeResult;
}

std::vector<SagaEngine::Replication::MovementDirtyReplicationIntent>
ZoneServer::ConsumeMovementDirtyReplicationIntentsForTesting()
{
    if (!m_movementDirtyReplicationBridge)
        return {};

    return m_movementDirtyReplicationBridge->ConsumeDirtyMovementIntents();
}

std::size_t
ZoneServer::GetPendingMovementDirtyReplicationIntentCountForTesting() const
{
    if (!m_movementDirtyReplicationBridge)
        return 0;

    return m_movementDirtyReplicationBridge->PendingCount();
}

void ZoneServer::RecordTickDurationForTesting(uint64_t tickIndex, uint64_t durationUs)
{
    RecordTickReliabilityDiagnostics(tickIndex, durationUs);
}

void ZoneServer::RecordServerStartedForTesting()
{
    RecordServerStartedLifecycle();
}

void ZoneServer::RecordSessionConnectedForTesting(ClientId clientId)
{
    RecordSessionConnectedLifecycle(clientId);
}

void ZoneServer::RecordSessionDisconnectedForTesting(ClientId clientId)
{
    RecordSessionDisconnectedLifecycle(clientId);
}

} // namespace SagaEngine::ServerAuthority
