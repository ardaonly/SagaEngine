#pragma once

/// @file InputCommandInbox.h
/// @brief Server-side per-client input command queue with tick-based ordering.
///
/// Layer  : Server / Input
/// Purpose: Receives InputCommands from the network layer and presents them
///          to the simulation tick by tick. Handles:
///            - Out-of-order arrival (sort by sequence)
///            - Duplicate detection (by sequence number)
///            - Missing command handling (return last valid or empty)
///            - Command window eviction (old commands are dropped)
///
/// Architecture:
///   Network recv thread  → PushReceived()
///   Simulation tick      → ConsumeForTick() / GetForTick()
///
///   The inbox is designed for MPSC: multiple receive paths (for UDP
///   redundant sending), single consumer (simulation tick).
///
/// Authority note:
///   The server NEVER trusts the command's simulationTick field blindly.
///   ConsumeForTick() is always called with the server's authoritative tick.
///   The client's simulationTick in the command is advisory only (for
///   latency compensation / input delay calculation).
///
/// Anti-cheat note:
///   The inbox enforces a maximum command rate per client.
///   Commands above kMaxCommandsPerTick are silently dropped.
///   Command values (moveX, buttons) are validated in ServerInputProcessor,
///   not here — separation of concerns.

#include "SagaEngine/Input/Commands/InputCommand.h"
#include <deque>
#include <mutex>
#include <optional>
#include <vector>
#include <cstdint>

namespace SagaServer::Input
{

inline constexpr size_t kMaxCommandsPerTick    = 4;   ///< Anti-flood per tick
inline constexpr size_t kInboxWindowSize       = 128; ///< Drop older than this
inline constexpr uint32_t kInvalidSequence     = 0;

class InputCommandInbox
{
public:
    explicit InputCommandInbox(uint32_t clientId)
        : m_clientId(clientId) {}

    ~InputCommandInbox() = default;
    InputCommandInbox(const InputCommandInbox&) = delete;
    InputCommandInbox& operator=(const InputCommandInbox&) = delete;

    // Network thread: receive

    /// Called by the network receive thread when a packet arrives.
    /// Thread-safe. Returns false if the command was rejected (duplicate,
    /// out-of-window, or rate-limited).
    [[nodiscard]] bool PushReceived(const SagaEngine::Input::InputCommand& cmd);

    // Simulation thread: consume

    /// Returns the best available command for a given server tick.
    /// If no command arrived in time, returns the last known command
    /// with a "late" flag set (server's choice to accept or use fallback).
    struct TickResult
    {
        std::optional<SagaEngine::Input::InputCommand> command;
        bool isLate     = false;    ///< No command arrived in time
        bool isDuplicate= false;    ///< Received but already processed
    };

    [[nodiscard]] TickResult ConsumeForTick(uint32_t serverTick);

    /// Acknowledge up to this sequence number.
    /// Sends ack info back to the client (via caller — we only store it).
    [[nodiscard]] uint32_t GetLastProcessedSequence() const noexcept
    {
        return m_lastProcessedSequence;
    }

    // Diagnostics

    [[nodiscard]] uint32_t GetClientId()           const noexcept { return m_clientId; }
    [[nodiscard]] size_t   GetPendingCount()        const;
    [[nodiscard]] uint32_t GetDroppedCommandCount() const noexcept { return m_droppedCount; }
    [[nodiscard]] uint32_t GetLateCommandCount()    const noexcept { return m_lateCount; }

    void Reset();

private:
    mutable std::mutex                                  m_mutex;
    uint32_t                                            m_clientId;
    uint32_t                                            m_lastProcessedSequence = 0;
    uint32_t                                            m_droppedCount          = 0;
    uint32_t                                            m_lateCount             = 0;

    /// Received but not yet consumed. Sorted by sequence ascending.
    std::deque<SagaEngine::Input::InputCommand>         m_queue;

    SagaEngine::Input::InputCommand                     m_lastCommand{};
    bool                                                m_hasLastCommand = false;
};

} // namespace SagaServer::Input