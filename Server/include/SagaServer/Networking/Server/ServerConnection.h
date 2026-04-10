/// @file ServerConnection.h
/// @brief Per-client connection state machine with auth, heartbeat, and send queue.
///
/// Layer  : Server / Networking / Server
/// Purpose: ServerConnection models one TCP connection from a game client.
///          It owns the socket, manages the connection lifecycle state machine
///          (Connecting → Handshaking → Connected → Draining → Disconnected),
///          drives the heartbeat timer, and exposes a thread-safe send queue.
///
/// Threading:
///   - Receive callbacks fire on an io_context thread.
///   - Send() is safe to call from any thread (tick or io thread).
///   - State machine transitions are protected by an internal strand.

#pragma once

#include "SagaServer/Networking/Core/NetworkTypes.h"
#include "SagaServer/Networking/Core/Packet.h"

#include <asio.hpp>
#include <asio/steady_timer.hpp>

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace SagaServer::Networking
{

// Bring engine networking types into scope
using SagaEngine::Networking::ClientId;
using SagaEngine::Networking::ConnectionId;
using SagaEngine::Networking::Packet;
using SagaEngine::Networking::PacketType;

// ─── ConnectionState ──────────────────────────────────────────────────────────

/// Linear state machine for a single client connection.
enum class ServerConnectionState : uint8_t
{
    Connecting    = 0,  ///< TCP accepted; no handshake yet
    Handshaking   = 1,  ///< Auth challenge / response in flight
    Connected     = 2,  ///< Fully authenticated, game traffic active
    Draining      = 3,  ///< Shutdown initiated; flushing send queue
    Disconnected  = 4,  ///< Socket closed; object may be recycled
};

// ─── ServerConnectionConfig ───────────────────────────────────────────────────

/// Tunable knobs for one connection.
struct ServerConnectionConfig
{
    uint32_t heartbeatIntervalMs{1000};       ///< Interval between server → client pings
    uint32_t connectionTimeoutMs{10000};      ///< Max silence before disconnect
    uint32_t handshakeTimeoutMs{5000};        ///< Max time to complete auth
    uint32_t maxSendQueueBytes{524288};       ///< Hard back-pressure cap (512 KiB)
    uint32_t receiveBufferBytes{65536};       ///< Per-connection receive ring size
    uint32_t maxConcurrentSends{16};          ///< Max outstanding async_write calls
};

// ─── ServerConnectionStats ────────────────────────────────────────────────────

/// Per-connection live metrics.
struct ServerConnectionStats
{
    std::atomic<uint64_t> bytesSent{0};
    std::atomic<uint64_t> bytesReceived{0};
    std::atomic<uint64_t> packetsSent{0};
    std::atomic<uint64_t> packetsReceived{0};
    std::atomic<uint64_t> sendQueueDrops{0};
    std::atomic<uint64_t> heartbeatsSent{0};
    std::atomic<uint64_t> retransmits{0};
    std::atomic<uint32_t> rttMs{0};
};

// ─── Callbacks ────────────────────────────────────────────────────────────────

using ConnectionPacketCallback    = std::function<void(ClientId, const uint8_t*, std::size_t)>;
using ConnectionStateCallback     = std::function<void(ClientId, ServerConnectionState)>;
using ConnectionDisconnectCallback = std::function<void(ClientId, int /*reason*/)>;

// ─── ServerConnection ─────────────────────────────────────────────────────────

/// Models a single authenticated game client connection.
class ServerConnection final : public std::enable_shared_from_this<ServerConnection>
{
public:
    ServerConnection(ClientId                   clientId,
                     asio::ip::tcp::socket      socket,
                     asio::io_context&          ioContext,
                     ServerConnectionConfig     config);

    ~ServerConnection();

    ServerConnection(const ServerConnection&)            = delete;
    ServerConnection& operator=(const ServerConnection&) = delete;

    // ── Lifecycle ─────────────────────────────────────────────────────────────

    /// Begin reading from the socket and start heartbeat / timeout timers.
    void Start();

    /// Gracefully drain the send queue, then close the socket.
    /// Calls the disconnect callback once the socket is closed.
    void Shutdown(int reason = 0);

    /// Forcibly close the socket without draining.
    void ForceClose(int reason = 0);

    // ── Send ──────────────────────────────────────────────────────────────────

    /// Queue a packet for async delivery. Thread-safe.
    /// Returns false if the send queue is at capacity (back-pressure).
    [[nodiscard]] bool Send(const uint8_t* data, std::size_t size);

    /// Queue a pre-built Packet for async delivery. Thread-safe.
    [[nodiscard]] bool Send(const Packet& packet);

    // ── Callbacks ─────────────────────────────────────────────────────────────

    void SetOnPacketReceived(ConnectionPacketCallback cb);
    void SetOnStateChanged(ConnectionStateCallback cb);
    void SetOnDisconnected(ConnectionDisconnectCallback cb);

    // ── Queries ───────────────────────────────────────────────────────────────

    [[nodiscard]] ClientId                GetClientId()     const noexcept;
    [[nodiscard]] ServerConnectionState   GetState()        const noexcept;
    [[nodiscard]] bool                    IsConnected()     const noexcept;
    [[nodiscard]] std::string             GetRemoteAddress() const;
    [[nodiscard]] const ServerConnectionStats& GetStats()   const noexcept;
    [[nodiscard]] std::chrono::steady_clock::time_point GetLastReceivedTime() const noexcept;

private:
    // ── Receive pipeline ──────────────────────────────────────────────────────

    void StartReceiveHeader();
    void HandleReceiveHeader(const asio::error_code& ec, std::size_t bytes);
    void StartReceiveBody(uint32_t bodyLength);
    void HandleReceiveBody(const asio::error_code& ec, std::size_t bytes, uint32_t bodyLength);
    void DispatchPacket(const uint8_t* data, std::size_t size);

    // ── Send pipeline ─────────────────────────────────────────────────────────

    void EnqueueSend(std::vector<uint8_t> data);
    void TryFlushSendQueue();
    void HandleSendComplete(const asio::error_code& ec, std::size_t bytes);

    // ── Heartbeat & timeout ───────────────────────────────────────────────────

    void StartHeartbeatTimer();
    void HandleHeartbeatTimer(const asio::error_code& ec);
    void StartTimeoutTimer();
    void HandleTimeoutTimer(const asio::error_code& ec);
    void StartHandshakeTimer();
    void HandleHandshakeTimer(const asio::error_code& ec);

    // ── Handshake ─────────────────────────────────────────────────────────────

    void SendHandshakeChallenge();
    void ProcessHandshakeResponse(const uint8_t* data, std::size_t size);

    // ── State transitions ─────────────────────────────────────────────────────

    void TransitionTo(ServerConnectionState newState);
    void HandleDisconnect(int reason);

    // ── Utilities ─────────────────────────────────────────────────────────────

    void SendHeartbeatPing();
    void SendHeartbeatAck(uint64_t pingTimestamp);
    [[nodiscard]] bool IsHealthy() const noexcept;

    // ── Members ───────────────────────────────────────────────────────────────

    ClientId                              m_clientId;
    asio::ip::tcp::socket                 m_socket;
    asio::strand<asio::io_context::executor_type> m_strand;
    ServerConnectionConfig                m_config;
    ServerConnectionStats                 m_stats;

    std::atomic<ServerConnectionState>    m_state{ServerConnectionState::Connecting};

    // ── Timers ────────────────────────────────────────────────────────────────

    asio::steady_timer  m_heartbeatTimer;
    asio::steady_timer  m_timeoutTimer;
    asio::steady_timer  m_handshakeTimer;

    // ── Receive buffers ───────────────────────────────────────────────────────

    static constexpr std::size_t kHeaderSize = 8; ///< PacketHeader wire size
    std::vector<uint8_t>         m_headerBuf;
    std::vector<uint8_t>         m_bodyBuf;

    // ── Send queue ────────────────────────────────────────────────────────────

    mutable std::mutex              m_sendMutex;
    std::deque<std::vector<uint8_t>> m_sendQueue;    ///< Pending outbound buffers
    std::size_t                      m_sendQueueBytes{0};
    bool                             m_sendActive{false};

    // ── Timing ────────────────────────────────────────────────────────────────

    std::atomic<std::chrono::steady_clock::time_point::rep> m_lastReceivedTime;
    std::chrono::steady_clock::time_point                   m_connectTime;
    uint64_t                                                m_lastPingTimestamp{0};

    // ── Callbacks ─────────────────────────────────────────────────────────────

    ConnectionPacketCallback     m_onPacketReceived;
    ConnectionStateCallback      m_onStateChanged;
    ConnectionDisconnectCallback m_onDisconnected;

    bool m_disconnectNotified{false};
};

} // namespace SagaServer::Networking
