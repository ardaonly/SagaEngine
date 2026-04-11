/// @file WorldSnapshotWire.h
/// @brief Engine-side decoder for server-produced full snapshot wire format.
///
/// Layer  : SagaEngine / Client / Replication
/// Purpose: Converts raw network bytes from a PacketType::Snapshot (24)
///          packet into a `DecodedWorldSnapshot` that the pipeline can
///          apply.  All bounds checking and CRC32 validation happens here.
///
/// Wire format (must match server-side WorldSnapshotCapture::Capture):
///   [SnapshotFileHeader: 88 bytes — extended with schemaVersion, protocolVersion]
///     magic(4) | version(4) | serverTick(8) | captureTime(8) |
///     zoneId(4) | shardId(4) | entityCount(4) | payloadCRC32(4) |
///     byteLength(8) | schemaVersion(4) | protocolVersion(4) | reserved(12)
///   [Per Entity:]
///     entityId(4) | componentCount(2)
///     [Per Component:]
///       typeId(2) | dataLen(2) | data[N]

#pragma once

#include "SagaEngine/Client/Replication/SnapshotApplyPipeline.h"

#include <cstdint>
#include <string>
#include <vector>

namespace SagaEngine::Client::Replication {

// ─── Wire constants ────────────────────────────────────────────────────────

/// Snapshot file magic: 'SGSW' (SagaEngine SnapShot World).
inline constexpr std::uint32_t kSnapshotMagic = 0x53475357u;

/// Snapshot wire version.
inline constexpr std::uint32_t kSnapshotWireVersion = 3;

/// Extended header size (88 bytes with schema + protocol version).
inline constexpr std::size_t kExtendedHeaderSize = 88;

/// Maximum entity count per snapshot (sanity limit).
inline constexpr std::uint32_t kMaxSnapshotEntities = 65536;

/// Maximum payload bytes per snapshot (2 MiB — full world state).
inline constexpr std::uint32_t kMaxSnapshotPayloadBytes = 2 * 1024 * 1024;

// ─── Decoded snapshot entity ───────────────────────────────────────────────

/// One entity within a decoded full snapshot.
struct SnapshotEntityRecord
{
    std::uint32_t entityId = 0;

    struct ComponentRef
    {
        std::uint16_t   typeId = 0;
        const std::uint8_t* data = nullptr;  ///< Non-owning, valid while payload lives.
        std::uint16_t   dataLen = 0;
    };
    std::vector<ComponentRef> components;
};

// ─── Decoded world snapshot ────────────────────────────────────────────────

/// In-memory form of a decoded full snapshot.  Produced by
/// `DecodeSnapshotWire` and consumed by the apply pipeline.
struct DecodedWorldWire
{
    ServerTick                     serverTick     = kInvalidTick;
    std::uint64_t                  captureTimeMs  = 0;
    std::uint32_t                  zoneId         = 0;
    std::uint32_t                  shardId        = 0;
    std::uint32_t                  entityCount    = 0;
    std::uint32_t                  schemaVersion  = 0;
    std::uint32_t                  protocolVersion = 0;
    std::uint32_t                  payloadCRC32   = 0;
    std::vector<SnapshotEntityRecord> entities;

    /// Raw payload buffer.  SnapshotEntityRecord::ComponentRef::data
    /// points into this vector.
    std::vector<std::uint8_t> payload;
};

// ─── Decode result ─────────────────────────────────────────────────────────

struct SnapshotDecodeResult
{
    bool                success = false;
    DecodedWorldWire    decoded;
    std::string         error;
};

// ─── Decode function ───────────────────────────────────────────────────────

/// Decode raw full snapshot bytes into a structured form.
///
/// Performs all bounds checks:
///   - Magic number and version validation
///   - entityCount ≤ kMaxSnapshotEntities
///   - byteLength ≤ kMaxSnapshotPayloadBytes
///   - CRC32 verification against payload
///   - Per-entity: componentCount sanity
///   - Per-component: dataLen ≤ remaining payload bytes
///   - Schema/protocol version extraction (for compatibility checks)
///
/// @param data       Raw packet payload (after PacketHeader strip).
/// @param size       Byte count of the payload.
/// @param maxTypeId  Maximum registered component type ID.
///                   typeId values above this are skipped.
[[nodiscard]] SnapshotDecodeResult DecodeSnapshotWire(
    const std::uint8_t* data,
    std::size_t         size,
    std::uint32_t       maxTypeId = 0xFFFFFFFFu);

/// Compute CRC32 of a byte range (IEEE 802.3 polynomial).
/// Used by the snapshot decoder to verify payload integrity.
[[nodiscard]] std::uint32_t ComputeCRC32(const std::uint8_t* data, std::size_t size) noexcept;

} // namespace SagaEngine::Client::Replication
