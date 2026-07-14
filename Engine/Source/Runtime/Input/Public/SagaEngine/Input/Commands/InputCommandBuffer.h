#pragma once

/// @file InputCommandBuffer.h
/// @brief Thread-safe client-side command buffer with sequence tracking and ack.
///
/// Layer  : Input / Commands
/// Purpose: Bridges the input thread (writes commands) and the network thread
///          (reads and sends commands). Also maintains an unacked window for
///          client-side prediction and retransmission.
///
/// Threading model:
///   Writer (input/main thread) → PushLocal()
///   Reader (network thread)    → PeekPending(), ConsumeForSend()
///   Reader (prediction)        → GetUnackedRange()
///   Writer (network thread)    → AckUpTo()
///
///   All public methods are mutex-protected. The buffer is designed for
///   low contention — command rate is 1/tick (~20-64 per second).
///
/// Unacked window:
///   Commands stay in the unacked window until AckUpTo(sequence) is called.
///   The client prediction system reuses these commands for reconciliation.
///   The network layer may resend unacked commands if the RTT window demands it.
///
/// Capacity policy:
///   kMaxUnackedCommands should cover worst-case RTT in ticks.
///   At 20 ticks/sec and 500ms RTT: 10 commands minimum.
///   Default: 256 covers ~12 seconds at 20hz or ~4 seconds at 64hz.

#include "SagaEngine/Input/Commands/InputCommand.h"
#include <deque>
#include <mutex>
#include <optional>
#include <span>
#include <vector>
#include <cstdint>

namespace SagaEngine::Input
{

inline constexpr size_t kMaxUnackedCommands = 256;

class InputCommandBuffer
{
public:
    InputCommandBuffer()  = default;
    ~InputCommandBuffer() = default;

    InputCommandBuffer(const InputCommandBuffer&) = delete;
    InputCommandBuffer& operator=(const InputCommandBuffer&) = delete;

    // Input thread: write

    /// Push a locally produced command. Returns false if the buffer is full
    /// (server has stopped acknowledging — likely a severe network issue).
    [[nodiscard]] bool PushLocal(const InputCommand& cmd);

    // Network thread: read

    /// Returns all pending (not yet sent this flush cycle) commands.
    /// Commands remain in the pending queue after this call.
    [[nodiscard]] std::vector<InputCommand> PeekPending() const;

    /// Mark the N oldest pending commands as sent (moves them to unacked only).
    /// Called after the network layer successfully enqueues them for send.
    void MarkSent(uint32_t upToSequence);

    // Network thread: acknowledgement

    /// Server has confirmed processing up to (and including) this sequence.
    /// Removes those commands from the unacked window.
    void AckUpTo(uint32_t sequence);

    // Prediction: read

    /// Snapshot of currently unacked commands (for prediction replay).
    /// Returns a copy — safe to read from any thread after this call.
    [[nodiscard]] std::vector<InputCommand> GetUnackedSnapshot() const;

    /// Find the command for a specific simulation tick, if present.
    [[nodiscard]] std::optional<InputCommand> FindByTick(uint32_t tick) const;

    // Diagnostics

    [[nodiscard]] size_t GetPendingCount()  const;
    [[nodiscard]] size_t GetUnackedCount()  const;
    [[nodiscard]] bool   IsFull()           const;

    /// Clear everything. Call on disconnect/reconnect.
    void Reset();

private:
    mutable std::mutex              m_mutex;

    /// Commands written locally, not yet sent to network layer.
    std::deque<InputCommand>        m_pending;

    /// Commands sent but not yet acknowledged by server.
    std::deque<InputCommand>        m_unacked;
};

} // namespace SagaEngine::Input