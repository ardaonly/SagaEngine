/// @file EventStream.h
/// @brief Append-only event log for world state — the backbone of event sourcing.
///
/// Layer  : SagaEngine / World
/// Purpose: World state is NOT mutable variables updated in place.  Every
///          state change is an immutable event appended to a log.  The
///          current world state is the fold (reduction) of all events.
///
///          This enables:
///            - Replay: replay events from any point in time to reconstruct state
///            - Debug: deterministic replay of any issue
///            - Analytics: stream of events feeds into analytics pipeline
///            - Audit: every state change is traceable
///            - Snapshots: periodic checkpoints reduce replay time
///
/// Design rules:
///   - Events are append-only; never modified after append
///   - Events are ordered by monotonic sequence number
///   - Snapshots are periodic full-state captures with a base sequence
///   - Replaying from snapshot + events after it is faster than from scratch
///   - The stream is single-writer (WorldNode simulation thread)
///   - Events are flushed to disk asynchronously via AppendBatch
///
/// What this is NOT:
///   - Not a message queue.  Events are historical records, not in-flight messages.
///   - Not a database.  Persistence is append-only with fsync guarantees.

#pragma once

#include "SagaEngine/World/SimCell.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include <optional>
#include <unordered_map>
#include <mutex>

namespace SagaEngine::World {

// ─── Event types ────────────────────────────────────────────────────────────────

/// Every world event has a type and a payload.  The payload is raw bytes
/// interpreted by the event type.
enum class WorldEventType : uint16_t
{
    // Cell lifecycle
    CellCreated       = 0x0001,
    CellLoaded        = 0x0002,
    CellActive        = 0x0003,
    CellDormant       = 0x0004,
    CellSplit         = 0x0005,
    CellMerge         = 0x0006,
    CellMigrate       = 0x0007,

    // Entity lifecycle
    EntitySpawned     = 0x0010,
    EntityDespawned   = 0x0011,
    EntityMoved       = 0x0012,

    // Domain simulation
    CombatTick        = 0x0020,
    MovementTick      = 0x0021,
    EconomyTick       = 0x0022,
    PoliticsTick      = 0x0023,
    EcologyTick       = 0x0024,

    // World events
    PlayerConnected   = 0x0030,
    PlayerDisconnected = 0x0031,
    SnapshotTaken     = 0x0040,
    NarrativeTrigger = 0x0050,
};

[[nodiscard]] inline const char* WorldEventTypeToString(WorldEventType t) noexcept
{
    switch (t)
    {
        case WorldEventType::CellCreated:       return "CellCreated";
        case WorldEventType::CellLoaded:        return "CellLoaded";
        case WorldEventType::CellActive:        return "CellActive";
        case WorldEventType::CellDormant:       return "CellDormant";
        case WorldEventType::CellSplit:         return "CellSplit";
        case WorldEventType::CellMerge:         return "CellMerge";
        case WorldEventType::CellMigrate:       return "CellMigrate";
        case WorldEventType::EntitySpawned:     return "EntitySpawned";
        case WorldEventType::EntityDespawned:   return "EntityDespawned";
        case WorldEventType::EntityMoved:       return "EntityMoved";
        case WorldEventType::CombatTick:        return "CombatTick";
        case WorldEventType::MovementTick:      return "MovementTick";
        case WorldEventType::EconomyTick:       return "EconomyTick";
        case WorldEventType::PoliticsTick:      return "PoliticsTick";
        case WorldEventType::EcologyTick:       return "EcologyTick";
        case WorldEventType::PlayerConnected:   return "PlayerConnected";
        case WorldEventType::PlayerDisconnected:return "PlayerDisconnected";
        case WorldEventType::SnapshotTaken:     return "SnapshotTaken";
        case WorldEventType::NarrativeTrigger:  return "NarrativeTrigger";
    }
    return "Unknown";
}

// ─── World event ────────────────────────────────────────────────────────────────

/// A single immutable event in the world's history.
struct WorldEvent
{
    uint64_t        sequence    = 0;   ///< Monotonic sequence number (global order).
    uint64_t        worldTick   = 0;   ///< World tick at the time of the event.
    uint64_t        timestampUs = 0;   ///< Wall-clock microseconds since epoch.
    WorldEventType  type        = WorldEventType::CellCreated;
    SimCellId       cellId{};           ///< Cell where the event occurred.
    uint32_t        entityId    = 0;    ///< Entity involved (0 = cell-level).
    uint32_t        data        = 0;    ///< Type-specific scalar payload.
    std::vector<uint8_t> payload;       ///< Type-specific blob payload.
};

// ─── Snapshot ───────────────────────────────────────────────────────────────────

/// A periodic full-state capture.  Replay from a snapshot + events after
/// its base sequence is much faster than replaying from scratch.
struct WorldSnapshot
{
    uint64_t        baseSequence  = 0;   ///< Last event sequence included.
    uint64_t        worldTick     = 0;
    uint64_t        timestampUs   = 0;
    std::vector<uint8_t> stateData;       ///< Serialized world state.
};

// ─── Event handler ──────────────────────────────────────────────────────────────

/// Called for each event during replay or live streaming.
using EventHandlerFn = std::function<void(const WorldEvent&)>;

// ─── Event stream ───────────────────────────────────────────────────────────────

/// Append-only event log with snapshot support.
///
/// Usage:
///   1. Create EventStream
///   2. Append events as world state changes
///   3. Periodically take snapshots
///   4. Replay from snapshot + remaining events to reconstruct state
///
/// Thread model: single-writer (WorldNode simulation thread).
class EventStream
{
public:
    EventStream() = default;
    ~EventStream() = default;

    EventStream(const EventStream&)            = delete;
    EventStream& operator=(const EventStream&) = delete;

    // ─── Event append ─────────────────────────────────────────────────────────

    /// Append an event to the stream.  Sequence is auto-assigned.
    /// The event's worldTick and timestampUs should be set by the caller.
    void Append(WorldEvent evt) noexcept;

    /// Bulk-append multiple events (more efficient than individual calls).
    void AppendBatch(const std::vector<WorldEvent>& events) noexcept;

    // ─── Snapshot management ──────────────────────────────────────────────────

    /// Take a snapshot of the current world state.
    /// The snapshot's baseSequence is set to the current last sequence.
    void TakeSnapshot(WorldSnapshot snap) noexcept;

    /// Get the latest snapshot, if any.
    [[nodiscard]] const WorldSnapshot* LatestSnapshot() const noexcept;

    // ─── Replay ───────────────────────────────────────────────────────────────

    /// Replay all events from the beginning (slow).
    void ReplayFromStart(EventHandlerFn handler) const noexcept;

    /// Replay from the latest snapshot + events after it (fast).
    void ReplayFromSnapshot(EventHandlerFn handler) const noexcept;

    /// Replay events in a specific sequence range.
    void ReplayRange(uint64_t fromSeq, uint64_t toSeq, EventHandlerFn handler) const noexcept;

    // ─── Queries ──────────────────────────────────────────────────────────────

    [[nodiscard]] uint64_t LastSequence()    const noexcept { return m_lastSequence; }
    [[nodiscard]] uint64_t EventCount()      const noexcept { return m_events.size(); }
    [[nodiscard]] uint64_t SnapshotCount()   const noexcept { return m_snapshots.size(); }

    /// Get event by sequence number (for debugging/replay).
    [[nodiscard]] const WorldEvent* GetEvent(uint64_t seq) const noexcept;

    /// Get all events since a given sequence (for live streaming to clients).
    [[nodiscard]] std::vector<WorldEvent> GetEventsAfter(uint64_t lastSeenSeq) const noexcept;

    // ─── Disk persistence ─────────────────────────────────────────────────────

    /// Open the event log file for appending.  Creates the file if it does
    /// not exist.  Returns false on I/O error.
    ///
    /// The file format is append-only binary:
    ///   [Event header: 32 bytes] [payload: variable]
    ///   header = sequence(8) | worldTick(8) | timestampUs(8) | type(2) |
    ///            cellX(2) | cellZ(2) | entityId(4) | data(4) | payloadLen(4)
    bool Open(const char* path) noexcept;

    /// Close the file and flush any pending writes.
    void Close() noexcept;

    /// Flush the in-memory event buffer to disk.  Called periodically by
    /// the WorldNode (typically every tick).  Uses fsync for durability.
    void FlushPending() noexcept;

    /// Load all events from disk into memory.  Called during WorldNode
    /// startup to reconstruct state from the event log.
    bool LoadFromFile() noexcept;

    /// Save the latest snapshot to disk.
    bool SaveSnapshot(const WorldSnapshot& snap) noexcept;

    /// Load the latest snapshot from disk.
    [[nodiscard]] std::optional<WorldSnapshot> LoadLatestSnapshot() const noexcept;

private:
    // ─── Internal helpers ─────────────────────────────────────────────────────

    /// Serialize a single event to binary form.
    [[nodiscard]] std::vector<uint8_t> SerializeEvent(const WorldEvent& evt) const noexcept;

    /// Deserialize a single event from binary.
    [[nodiscard]] static std::optional<WorldEvent> DeserializeEvent(
        const uint8_t* data, size_t size) noexcept;

    // ─── Members ──────────────────────────────────────────────────────────────

    std::vector<WorldEvent>   m_events;
    std::vector<WorldSnapshot> m_snapshots;
    uint64_t                  m_lastSequence = 0;

    // Disk persistence.
    void*                     m_fileHandle = nullptr; ///< Opaque FILE* (platform-specific).
    std::vector<uint8_t>      m_writeBuffer;          ///< Buffered events before flush.
    static constexpr size_t   kFlushThreshold = 64 * 1024; ///< 64 KiB flush trigger.
    char                      m_filePath[512] = {};    ///< Copy of the open path.
};

/// Configuration for EventStream disk I/O.
struct EventStreamConfig
{
    const char* logPath      = "events.log";   ///< Append-only event log file.
    const char* snapshotPath = "snapshot.bin";  ///< Latest snapshot file.
    bool        fsyncOnFlush = true;            ///< Durability vs performance trade-off.
    size_t      flushBytes   = 64 * 1024;       ///< Flush when buffer exceeds this.
};

} // namespace SagaEngine::World
