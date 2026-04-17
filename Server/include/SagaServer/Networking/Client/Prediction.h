/// @file Prediction.h
/// @brief Server-side client prediction bookkeeping — authoritative input reconciliation.
///
/// Layer  : SagaServer / Networking / Client
/// Purpose: The server needs to track what input commands each client has sent,
///          which commands the server has already processed, and which commands
///          are still pending.  When the server processes a client's input command,
///          it must:
///            1. Validate the command's sequence number (no replays, no gaps)
///            2. Apply the command to the authoritative world state
///            3. Record the result so it can be included in the next snapshot
///            4. Send the confirmed command back to the client for reconciliation
///
///          This bookkeeping is the server-side mirror of the client's
///          ClientPrediction<T> class.  The client predicts locally; the server
///          confirms or corrects; the client reconciles on mismatch.
///
/// Design rules:
///   - Server is authoritative: if server and client disagree, server wins
///   - Commands are processed in strict sequence order (no reordering)
///   - Out-of-order or duplicate commands are rejected with a status code
///   - The server tracks a small window of recent commands for reconciliation
///   - Stale commands (older than the client's last confirmed tick) are discarded

#pragma once

#include "SagaEngine/ECS/Entity.h"

#include <array>
#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace SagaServer::Networking {

// ─── Input sequence numbers ─────────────────────────────────────────────

/// Monotonic sequence number for client input commands.
/// Each client maintains its own sequence counter; the server validates
/// that commands arrive in order.
using InputSequence = uint32_t;
inline constexpr InputSequence kInvalidSequence = 0;

/// Server tick at which a command was processed.
using ServerTick = uint64_t;
inline constexpr ServerTick kInvalidTick = 0;

// ─── Client input command ───────────────────────────────────────────────

/// A single input command from a client.  Commands are small and fixed-size
/// so they can be sent at high frequency (60 Hz) without bandwidth pressure.
struct ClientInputCommand
{
    InputSequence sequence    = kInvalidSequence;  ///< Client-local sequence number.
    ServerTick    clientTick  = kInvalidTick;       ///< Client's predicted tick when input was generated.
    uint32_t      entityId    = 0;                  ///< Entity the command targets (player entity).
    uint8_t       commandType = 0;                  ///< Command discriminator (move, jump, attack, etc.).
    uint8_t       payload[12] = {};                 ///< Command-specific data (direction, magnitude, etc.).
};

// ─── Command processing status ──────────────────────────────────────────

enum class CommandStatus : uint8_t
{
    Accepted,       ///< Command was valid and processed.
    Duplicate,      ///< Command sequence already processed (retransmit).
    TooOld,         ///< Command sequence is behind the server's processed window.
    TooNew,         ///< Command sequence is ahead (gap detected; waiting for fill).
    InvalidEntity,  ///< Command targets an entity the client doesn't control.
    RateLimited,    ///< Client exceeded input rate limit.
    InternalError,  ///< Server-side processing failure.
};

/// Human-readable status name.
[[nodiscard]] inline const char* CommandStatusToString(CommandStatus status) noexcept
{
    switch (status)
    {
        case CommandStatus::Accepted:      return "Accepted";
        case CommandStatus::Duplicate:     return "Duplicate";
        case CommandStatus::TooOld:        return "TooOld";
        case CommandStatus::TooNew:        return "TooNew";
        case CommandStatus::InvalidEntity: return "InvalidEntity";
        case CommandStatus::RateLimited:   return "RateLimited";
        case CommandStatus::InternalError: return "InternalError";
    }
    return "Unknown";
}

// ─── Command result ─────────────────────────────────────────────────────

/// Result of processing a client input command on the server.
struct CommandResult
{
    InputSequence   sequence      = kInvalidSequence;  ///< Echo of the command's sequence.
    ServerTick      serverTick    = kInvalidTick;       ///< Tick at which the server processed it.
    CommandStatus   status        = CommandStatus::InternalError;
    uint8_t         resultFlags   = 0;                  ///< Handler-specific result bits.
};

// ─── Server-side prediction tracker ─────────────────────────────────────

/// Tracks a single client's input commands on the server.
///
/// The server maintains a sliding window of recently processed commands so
/// that when the client sends a reconciliation request, the server can
/// respond with the authoritative results for those commands.
///
/// Usage (per client):
///   1. Client sends ClientInputCommand with sequence N
///   2. Server calls tracker.ProcessCommand(command) → CommandResult
///   3. If Accepted, the command is applied to the world state
///   4. Server includes the CommandResult in the next replication snapshot
///   5. Client receives the result and reconciles its predicted state
///
/// Thread model:
///   Each tracker is owned by one client session; no internal locking.
///   The ClientPredictionBookkeeper (below) manages multiple trackers with a mutex.
class ClientPredictionTracker
{
public:
    explicit ClientPredictionTracker(uint64_t clientId) noexcept
        : m_clientId(clientId)
    {
    }

    ~ClientPredictionTracker() noexcept = default;

    ClientPredictionTracker(const ClientPredictionTracker&)            = delete;
    ClientPredictionTracker& operator=(const ClientPredictionTracker&) = delete;

    /// Process an incoming input command from the client.
    /// @param command     The client's input command.
    /// @param serverTick  Current server tick.
    /// @param handler     Callback to apply the command to the world state.
    ///                    Called only if the command is valid and in sequence.
    /// @return CommandResult with status and server tick.
    using CommandApplyFn = std::function<void(const ClientInputCommand& command)>;
    [[nodiscard]] CommandResult ProcessCommand(const ClientInputCommand& command,
                                                ServerTick serverTick,
                                                CommandApplyFn handler) noexcept;

    /// Get the highest sequence number the server has processed for this client.
    [[nodiscard]] InputSequence HighestProcessed() const noexcept { return m_highestProcessed; }

    /// Get the number of commands in the recent history buffer.
    [[nodiscard]] uint32_t HistorySize() const noexcept
    {
        return kHistorySize;
    }

    /// Get the command result for a specific sequence (for reconciliation).
    /// @param sequence  The client's input sequence to look up.
    /// @return Pointer to the result, or nullptr if not found.
    [[nodiscard]] const CommandResult* GetResult(InputSequence sequence) const noexcept;

    /// Client ID this tracker belongs to.
    [[nodiscard]] uint64_t ClientId() const noexcept { return m_clientId; }

    /// Reset the tracker (e.g., on client reconnect).
    void Reset() noexcept;

    /// Controlled entity ID (set by the session manager).
    [[nodiscard]] uint32_t ControlledEntity() const noexcept { return m_controlledEntity; }
    void SetControlledEntity(uint32_t entityId) noexcept { m_controlledEntity = entityId; }

    static constexpr uint32_t kHistorySize = 64;  ///< Keep last 64 command results.

private:

    uint64_t         m_clientId          = 0;
    uint32_t         m_controlledEntity  = 0;
    InputSequence    m_highestProcessed  = kInvalidSequence;
    uint32_t         m_historyIndex      = 0;  ///< Circular buffer write index.
    CommandResult    m_history[kHistorySize] = {};  ///< Circular buffer of recent results.
};

// ─── Server-side prediction bookkeeper ──────────────────────────────────

/// Manages ClientPredictionTracker instances for all connected clients.
///
/// The bookkeeper is the central point where the server records which
/// client commands have been processed, and where the replication layer
/// queries for command results to include in snapshots.
///
/// Usage:
///   1. On client connect: bookkeeper.CreateTracker(clientId)
///   2. On client input: bookkeeper.ProcessCommand(clientId, command, tick, handler)
///   3. On replication: bookkeeper.GetResults(clientId, sinceSequence, outResults)
///   4. On client disconnect: bookkeeper.RemoveTracker(clientId)
///
/// Thread model:
///   All methods are mutex-protected for safety across network threads.
class ClientPredictionBookkeeper
{
public:
    ClientPredictionBookkeeper() noexcept = default;
    ~ClientPredictionBookkeeper() noexcept = default;

    ClientPredictionBookkeeper(const ClientPredictionBookkeeper&)            = delete;
    ClientPredictionBookkeeper& operator=(const ClientPredictionBookkeeper&) = delete;

    /// Create a new tracker for a connected client.
    /// @param clientId  Unique client session ID.
    /// @return Pointer to the tracker (owned by the bookkeeper; do not delete).
    ClientPredictionTracker* CreateTracker(uint64_t clientId) noexcept;

    /// Remove a tracker for a disconnected client.
    /// @param clientId  Client session ID.
    void RemoveTracker(uint64_t clientId) noexcept;

    /// Process a client input command.
    /// @param clientId   Client session ID.
    /// @param command    Input command from the client.
    /// @param serverTick Current server tick.
    /// @param handler    Callback to apply the command to world state.
    /// @return CommandResult with processing status.
    [[nodiscard]] CommandResult ProcessCommand(
        uint64_t clientId, const ClientInputCommand& command, ServerTick serverTick,
        ClientPredictionTracker::CommandApplyFn handler) noexcept;

    /// Get command results for a client since a given sequence.
    /// @param clientId      Client session ID.
    /// @param sinceSequence Start of the range (exclusive; results > this value).
    /// @param outResults    Output: vector of command results (appended to).
    /// @return true if the client has a tracker; false otherwise.
    bool GetResults(uint64_t clientId, InputSequence sinceSequence,
                    std::vector<CommandResult>& outResults) const noexcept;

    /// Get the highest processed sequence for a client.
    [[nodiscard]] InputSequence GetHighestProcessed(uint64_t clientId) const noexcept;

    /// Reset all trackers (e.g., on server restart).
    void ResetAll() noexcept;

    /// Number of active trackers.
    [[nodiscard]] uint32_t TrackerCount() const noexcept;

private:
    mutable std::mutex m_mutex;
    std::unordered_map<uint64_t, std::unique_ptr<ClientPredictionTracker>> m_trackers;
};

} // namespace SagaServer::Networking
