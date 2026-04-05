#pragma once

/// @file InputCommandInbox.h
/// @brief Server-side per-client input command queue with tick-based ordering.
///
/// Layer  : SagaServer / Input
/// Purpose: Receives deserialized InputCommands from the network layer and
///          presents them to the simulation tick by tick. Handles:
///            - Out-of-order arrival (sorted by sequence ascending)
///            - Duplicate detection (by sequence number)
///            - Missing command handling (return last valid command)
///            - Command window eviction (evict commands older than window)
///
/// Threading model:
///   Network recv thread  → PushReceived()   (one writer at a time per inbox)
///   Simulation thread    → ConsumeForTick() (single consumer, never concurrent)
///
///   PushReceived() is mutex-protected.
///   ConsumeForTick() assumes exclusive access from the simulation thread.
///   Do NOT call ConsumeForTick() concurrently with itself.
///
/// Identity:
///   Uses ConnectionId (uint64_t) from SagaEngine::Networking::NetworkTypes
///   to stay aligned with the transport layer's client identity.
///
/// Authority note:
///   The server NEVER trusts cmd.simulationTick blindly.
///   ConsumeForTick() is always called by the server with its authoritative tick.
///   The client's tick field is used only for latency diagnostics.
///
/// Anti-cheat:
///   kMaxCommandsPerTick enforces max command rate per client per tick.
///   Value clamping and button validation happen in ServerInputProcessor,
///   not here. This class is purely a queue with ordering guarantees.

#include "SagaEngine/Input/Commands/InputCommand.h"
#include "SagaServer/Networking/Core/NetworkTypes.h"
#include <deque>
#include <mutex>
#include <optional>
#include <vector>
#include <cstdint>

namespace SagaEngine::Input
{

/// Maximum commands we buffer per client. At 64hz with 500ms RTT = 32 commands.
/// 128 covers pathological latency spikes without unbounded memory growth.
inline constexpr size_t kInboxCapacity         = 128;

/// Maximum commands accepted from one client in a single server tick.
/// Prevents flood attacks where a client sends thousands of commands per tick.
inline constexpr size_t kMaxCommandsPerTick    = 4;

class InputCommandInbox
{
public:
    explicit InputCommandInbox(SagaEngine::Networking::ConnectionId connectionId);
    ~InputCommandInbox() = default;

    InputCommandInbox(const InputCommandInbox&) = delete;
    InputCommandInbox& operator=(const InputCommandInbox&) = delete;

    // ── Network thread: receive ───────────────────────────────────────────────

    enum class PushResult : uint8_t
    {
        Accepted    = 0,
        Duplicate   = 1,   ///< sequence already processed or in queue
        OutOfWindow = 2,   ///< sequence too far behind last processed
        QueueFull   = 3,   ///< inbox at capacity — client is flooding
        RateLimited = 4,   ///< too many commands this tick
    };

    /// Thread-safe. Called by the network recv thread when a packet is decoded.
    /// Returns the push result for diagnostics/logging.
    PushResult PushReceived(const SagaEngine::Input::InputCommand& cmd);

    // ── Simulation thread: consume ────────────────────────────────────────────

    struct TickResult
    {
        std::optional<SagaEngine::Input::InputCommand> command;
        bool isLate      = false;   ///< no command arrived for this tick in time
        bool usedFallback= false;   ///< returned last known command as fallback
    };

    /// Called once per simulation tick from the simulation thread.
    /// Picks the best available command for serverTick:
    ///   1. If a command is queued: pop oldest, return it.
    ///   2. If no command: return last known command with usedFallback=true.
    ///   3. If no history: return empty TickResult with isLate=true.
    /// Updates m_lastProcessedSequence for ack generation.
    [[nodiscard]] TickResult ConsumeForTick(uint32_t serverTick);

    // ── Ack support ───────────────────────────────────────────────────────────

    /// Returns the sequence number of the last successfully consumed command.
    /// The network layer uses this to build InputAck packets.
    [[nodiscard]] uint32_t GetLastProcessedSequence() const noexcept
    {
        return m_lastProcessedSequence;
    }

    // ── Diagnostics ───────────────────────────────────────────────────────────

    [[nodiscard]] SagaEngine::Networking::ConnectionId GetConnectionId() const noexcept
    {
        return m_connectionId;
    }

    [[nodiscard]] size_t   GetQueueDepth()           const;
    [[nodiscard]] uint32_t GetTotalReceived()         const noexcept { return m_totalReceived; }
    [[nodiscard]] uint32_t GetDuplicateCount()        const noexcept { return m_duplicateCount; }
    [[nodiscard]] uint32_t GetDroppedCount()          const noexcept { return m_droppedCount; }
    [[nodiscard]] uint32_t GetLateCount()             const noexcept { return m_lateCount; }
    [[nodiscard]] uint32_t GetFallbackCount()         const noexcept { return m_fallbackCount; }

    /// Reset all state. Call on reconnect.
    void Reset();

private:
    /// Insert cmd into m_queue in ascending sequence order.
    void InsertSorted(const SagaEngine::Input::InputCommand& cmd);

    /// Returns true if sequence is within the acceptance window.
    [[nodiscard]] bool IsInWindow(uint32_t sequence) const noexcept;

    SagaEngine::Networking::ConnectionId        m_connectionId;

    mutable std::mutex                          m_mutex;

    /// Commands waiting to be consumed. Sorted by sequence ascending.
    std::deque<SagaEngine::Input::InputCommand> m_queue;

    /// Last command returned to simulation. Used as fallback on late arrival.
    SagaEngine::Input::InputCommand             m_lastCommand{};
    bool                                        m_hasLastCommand = false;

    uint32_t m_lastProcessedSequence = 0;
    uint32_t m_commandsThisTick      = 0;   ///< reset each ConsumeForTick call

    // Diagnostics (atomic-free: written only from guarded paths)
    uint32_t m_totalReceived  = 0;
    uint32_t m_duplicateCount = 0;
    uint32_t m_droppedCount   = 0;
    uint32_t m_lateCount      = 0;
    uint32_t m_fallbackCount  = 0;
};

} // namespace SagaEngine::Input