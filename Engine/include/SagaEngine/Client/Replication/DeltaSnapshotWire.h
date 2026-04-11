/// @file DeltaSnapshotWire.h
/// @brief Engine-side decoder for server-produced delta snapshot wire format.
///
/// Layer  : SagaEngine / Client / Replication
/// Purpose: Converts raw network bytes from a PacketType::DeltaSnapshot
///          (25) packet into a `DecodedDeltaSnapshot` that the pipeline
///          can apply.  All bounds checking happens here — malformed or
///          malicious payloads are rejected before they reach the ECS.
///
/// Wire format (must match server-side DeltaSnapshotBuilder::Finalize):
///   [DeltaSnapshotHeader: 40 bytes]
///     version(4) | serverTick(8) | sequence(8) | ackSequence(8) |
///     entityCount(4) | payloadBytes(4) | schemaVersion(4)
///   [Per Entity:]
///     entityId(4) | componentCount(2)  -- 0xFFFF = tombstone
///     [Per Component:]
///       typeId(2) | dataLen(2) | data[N]

#pragma once

#include "SagaEngine/Client/Replication/SnapshotApplyPipeline.h"

#include <cstdint>
#include <string>
#include <vector>

namespace SagaEngine::Client::Replication {

// ─── Wire constants ────────────────────────────────────────────────────────

/// Delta snapshot version shipped on the wire.
inline constexpr std::uint32_t kDeltaWireVersion = 1;

/// Maximum entity count per delta (sanity limit).
inline constexpr std::uint32_t kMaxDeltaEntities = 65536;

/// Maximum component count per entity (sanity limit).
inline constexpr std::uint32_t kMaxComponentsPerEntity = 128;

/// Maximum payload bytes per delta (256 KiB).
inline constexpr std::uint32_t kMaxDeltaPayloadBytes = 256 * 1024;

/// Tombstone component count marker in wire format.
inline constexpr std::uint16_t kWireTombstoneMarker = 0xFFFF;

// ─── Decoded entity delta ──────────────────────────────────────────────────

/// One entity's changes within a delta snapshot.
struct EntityDeltaRecord
{
    std::uint32_t entityId = 0;
    bool          isTombstone = false;
    bool          isNew = false;

    /// Component type IDs and their raw data blobs.
    /// Non-owning pointers reference the parent payload buffer.
    struct ComponentRef
    {
        std::uint16_t   typeId = 0;
        const std::uint8_t* data = nullptr;  ///< Non-owning, valid while payload lives.
        std::uint16_t   dataLen = 0;
    };
    std::vector<ComponentRef> components;
};

// ─── Decoded delta snapshot ────────────────────────────────────────────────

/// In-memory form of a decoded delta snapshot.  Produced by
/// `DecodeDeltaWire` and consumed by the apply pipeline.
struct DecodedDeltaWire
{
    ServerTick                      serverTick   = kInvalidTick;
    ServerTick                      baselineTick = kInvalidTick;  ///< Derived from ackSequence.
    std::uint64_t                   sequence     = 0;
    std::uint64_t                   ackSequence  = 0;
    std::uint32_t                   schemaVersion = 0;
    std::uint32_t                   entityCount  = 0;
    std::vector<EntityDeltaRecord>  entities;

    /// Raw payload buffer.  EntityDeltaRecord::ComponentRef::data
    /// points into this vector; do not mutate or destroy while
    /// records are in use.
    std::vector<std::uint8_t> payload;
};

// ─── Decode result ─────────────────────────────────────────────────────────

/// Outcome of a wire decode attempt.
struct DeltaDecodeResult
{
    bool              success = false;
    DecodedDeltaWire  decoded;
    std::string       error;  ///< Populated on failure.
};

// ─── Decode function ───────────────────────────────────────────────────────

/// Decode raw delta snapshot bytes into a structured form.
///
/// Performs all bounds checks:
///   - Header size and version validation
///   - entityCount ≤ kMaxDeltaEntities
///   - payloadBytes ≤ kMaxDeltaPayloadBytes
///   - Per-entity: componentCount ≤ kMaxComponentsPerEntity
///   - Per-component: dataLen ≤ remaining payload bytes
///   - Total decoded bytes ≤ declared payloadBytes
///
/// @param data       Raw packet payload (after PacketHeader strip).
/// @param size       Byte count of the payload.
/// @param maxTypeId  Maximum registered component type ID (from ComponentRegistry).
///                   typeId values above this are skipped (forward-compatible).
[[nodiscard]] DeltaDecodeResult DecodeDeltaWire(
    const std::uint8_t* data,
    std::size_t         size,
    std::uint32_t       maxTypeId = 0xFFFFFFFFu);

} // namespace SagaEngine::Client::Replication
