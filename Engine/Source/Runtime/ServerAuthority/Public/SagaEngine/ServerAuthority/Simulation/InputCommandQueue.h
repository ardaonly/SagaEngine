/// @file InputCommandQueue.h
/// @brief Per-player server-side input command buffer with sequencing and lag compensation.
///
/// Layer  : SagaServer / Simulation
/// Purpose: Client input arrives on the network thread, gets framed as
///          InputCommand packets, and must then be consumed deterministically
///          by the simulation tick. InputCommandQueue sits between those two
///          worlds:
///            - The network thread pushes commands into a bounded ring.
///            - The tick thread drains commands in sequence order for the
///              current server tick, honoring a configurable jitter budget.
///          The queue de-duplicates retransmits by sequence id, rejects out-of-
///          window commands, and records statistics for replication quality
///          monitoring and anti-cheat correlation.
///
/// Threading:
///   - Every public operation is safe under concurrent producer/consumer use.
///   - A single spin-guarded mutex covers the hot path; ring traversal never
///     allocates under the lock.

#pragma once

#include "SagaServer/Networking/Core/NetworkTypes.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <unordered_map>
#include <vector>

namespace SagaEngine::Server::Simulation
{

using ClientId = ::SagaEngine::Networking::ClientId;

// ─── InputCommand ─────────────────────────────────────────────────────────────

/// Normalised movement/action intent produced by a client for a single tick.
/// The payload is a transparent byte blob so gameplay code can layer richer
/// schemas on top without touching the queue.
struct InputCommand
{
    std::uint64_t sequence{0};        ///< Monotonic per-client command id.
    std::uint64_t clientTick{0};      ///< Client-side tick that produced this command.
    std::uint64_t serverRecvTick{0};  ///< Tick on which the server received the command.
    std::uint64_t recvTimeUnixMs{0};  ///< Wall-clock receipt timestamp.

    float moveX{0.0f};
    float moveY{0.0f};
    float moveZ{0.0f};
    float lookYaw{0.0f};
    float lookPitch{0.0f};

    std::uint32_t buttonMask{0};      ///< Bitmask of pressed action buttons.
    std::uint32_t dtMicros{0};        ///< Client frame delta in microseconds.

    std::array<std::uint8_t, 32> payload{}; ///< Optional gameplay-specific data.
    std::uint8_t                 payloadSize{0};
};

// ─── Configuration ────────────────────────────────────────────────────────────

/// Tuning parameters for InputCommandQueue, shared by every client slot.
struct InputQueueConfig
{
    std::size_t   capacity{64};              ///< Ring buffer depth per client.
    std::uint32_t maxJitterTicks{8};         ///< Tolerated ahead-of-server window.
    std::uint32_t maxLagTicks{32};           ///< Tolerated behind-server window.
    std::uint32_t duplicateWindow{256};      ///< Remembered sequences for dedup.
    bool          rejectOutOfOrder{false};   ///< If true, discard late sequences.
};

// ─── Statistics ───────────────────────────────────────────────────────────────

/// Per-client counters used by telemetry and anti-cheat.
struct InputQueueStats
{
    std::uint64_t enqueued{0};
    std::uint64_t dequeued{0};
    std::uint64_t duplicates{0};
    std::uint64_t dropped{0};
    std::uint64_t outOfWindow{0};
    std::uint64_t overruns{0};
    std::uint64_t lastEnqueueUnixMs{0};
    std::uint64_t lastDequeueUnixMs{0};
};

// ─── InputCommandQueue ────────────────────────────────────────────────────────

/// Fixed-capacity, sequence-ordered input ring serving one client.
class InputCommandQueue final
{
public:
    explicit InputCommandQueue(ClientId clientId, InputQueueConfig config = {});
    ~InputCommandQueue() = default;

    InputCommandQueue(const InputCommandQueue&)            = delete;
    InputCommandQueue& operator=(const InputCommandQueue&) = delete;

    /// Push a command received on the network thread. Returns true when the
    /// command is accepted into the ring.
    [[nodiscard]] bool Enqueue(const InputCommand& cmd);

    /// Drain the next runnable command for `serverTick`. Returns nullopt when
    /// the queue has nothing ready to execute at the given tick.
    [[nodiscard]] std::optional<InputCommand> Dequeue(std::uint64_t serverTick);

    /// Drain every runnable command at once; honours max per-tick budget.
    [[nodiscard]] std::vector<InputCommand> DrainForTick(std::uint64_t serverTick,
                                                          std::size_t   maxCount);

    /// Drop every buffered command. Statistics are preserved.
    void Flush();

    [[nodiscard]] ClientId       GetClientId()   const noexcept { return m_ClientId; }
    [[nodiscard]] std::size_t    GetPending()    const;
    [[nodiscard]] InputQueueStats GetStatistics() const;
    [[nodiscard]] std::uint64_t  GetLastDequeuedSequence() const;

private:
    // ── Internal helpers ──────────────────────────────────────────────────────

    [[nodiscard]] bool IsDuplicateLocked(std::uint64_t sequence) const noexcept;
    void               RecordSequenceLocked(std::uint64_t sequence);
    [[nodiscard]] bool IsInWindowLocked(std::uint64_t commandTick,
                                         std::uint64_t serverTick) const noexcept;

    const ClientId        m_ClientId;
    const InputQueueConfig m_Config;

    mutable std::mutex    m_Mutex;
    std::vector<InputCommand> m_Ring;      ///< Sorted by (sequence) under the lock.
    std::uint64_t          m_LastDequeuedSeq{0};
    std::unordered_map<std::uint64_t, std::uint8_t> m_SeenSequences;
    InputQueueStats        m_Stats;
};

// ─── InputCommandRouter ───────────────────────────────────────────────────────

/// Fan-out helper that owns a queue per client and routes inbound packets by
/// ClientId. The router is what ZoneServer passes to the replication layer
/// when it wants to grant a specific client a command slot.
class InputCommandRouter final
{
public:
    explicit InputCommandRouter(InputQueueConfig defaults = {});
    ~InputCommandRouter() = default;

    InputCommandRouter(const InputCommandRouter&)            = delete;
    InputCommandRouter& operator=(const InputCommandRouter&) = delete;

    /// Create a queue slot for `clientId`. Returns false if one already exists.
    bool AddClient(ClientId clientId);

    /// Tear down a queue slot and flush any buffered commands.
    void RemoveClient(ClientId clientId);

    [[nodiscard]] bool HasClient(ClientId clientId) const;

    /// Enqueue a command into the routing slot for `clientId`.
    [[nodiscard]] bool Enqueue(ClientId clientId, const InputCommand& cmd);

    /// Drain the next runnable command for one client.
    [[nodiscard]] std::optional<InputCommand> Dequeue(ClientId     clientId,
                                                       std::uint64_t serverTick);

    /// Drain up to `maxPerClient` commands for each known client into `outByClient`.
    /// Returns the total number of commands emitted.
    std::size_t DrainAll(std::uint64_t serverTick,
                         std::size_t   maxPerClient,
                         std::unordered_map<ClientId, std::vector<InputCommand>>& outByClient);

    [[nodiscard]] std::vector<ClientId> ListClients() const;
    [[nodiscard]] InputQueueStats        GetStatistics(ClientId clientId) const;

private:
    InputQueueConfig  m_Defaults;
    mutable std::mutex m_Mutex;
    std::unordered_map<ClientId, std::unique_ptr<InputCommandQueue>> m_Queues;
};

} // namespace SagaEngine::Server::Simulation
