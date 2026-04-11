/// @file DeltaSnapshotWire.cpp
/// @brief Engine-side decoder for server-produced delta snapshot wire format.

#include "SagaEngine/Client/Replication/DeltaSnapshotWire.h"

#include "SagaEngine/Core/Log/Log.h"

#include <cstring>

namespace SagaEngine::Client::Replication {

namespace {

constexpr const char* kLogTag = "Replication";

// ─── Little-endian readers ─────────────────────────────────────────────────

[[nodiscard]] std::uint32_t ReadLE32(const std::uint8_t* p) noexcept
{
    return static_cast<std::uint32_t>(p[0])
         | (static_cast<std::uint32_t>(p[1]) << 8)
         | (static_cast<std::uint32_t>(p[2]) << 16)
         | (static_cast<std::uint32_t>(p[3]) << 24);
}

[[nodiscard]] std::uint64_t ReadLE64(const std::uint8_t* p) noexcept
{
    return static_cast<std::uint64_t>(ReadLE32(p))
         | (static_cast<std::uint64_t>(ReadLE32(p + 4)) << 32);
}

[[nodiscard]] std::uint16_t ReadLE16(const std::uint8_t* p) noexcept
{
    return static_cast<std::uint16_t>(p[0])
         | (static_cast<std::uint16_t>(p[1]) << 8);
}

} // namespace

// ─── Delta header wire size ────────────────────────────────────────────────

// version(4) + serverTick(8) + sequence(8) + ackSequence(8) +
// entityCount(4) + payloadBytes(4) + schemaVersion(4) = 40 bytes.
inline constexpr std::size_t kDeltaHeaderSize = 40;

// ─── Decode ────────────────────────────────────────────────────────────────

DeltaDecodeResult DecodeDeltaWire(
    const std::uint8_t* data,
    std::size_t         size,
    std::uint32_t       maxTypeId)
{
    DeltaDecodeResult result;

    // Bounds check: minimum header size.
    if (size < kDeltaHeaderSize)
    {
        result.error = "DeltaSnapshot: payload too small for header";
        return result;
    }

    // Parse header fields.
    std::uint32_t version      = ReadLE32(data + 0);
    std::uint64_t serverTick   = ReadLE64(data + 4);
    std::uint64_t sequence     = ReadLE64(data + 12);
    std::uint64_t ackSequence  = ReadLE64(data + 20);
    std::uint32_t entityCount  = ReadLE32(data + 28);
    std::uint32_t payloadBytes = ReadLE32(data + 32);
    std::uint32_t schemaVer    = (size >= kDeltaHeaderSize) ? ReadLE32(data + 36) : 0;

    // Version check.
    if (version != kDeltaWireVersion)
    {
        result.error = "DeltaSnapshot: unsupported wire version " + std::to_string(version);
        return result;
    }

    // Sanity checks.
    if (entityCount > kMaxDeltaEntities)
    {
        LOG_WARN(kLogTag,
                 "DeltaSnapshot: entity count %u exceeds max %u",
                 static_cast<unsigned>(entityCount),
                 static_cast<unsigned>(kMaxDeltaEntities));
        result.error = "DeltaSnapshot: entity count exceeds sanity limit";
        return result;
    }

    if (payloadBytes > kMaxDeltaPayloadBytes)
    {
        LOG_WARN(kLogTag,
                 "DeltaSnapshot: payload %u bytes exceeds max %u",
                 static_cast<unsigned>(payloadBytes),
                 static_cast<unsigned>(kMaxDeltaPayloadBytes));
        result.error = "DeltaSnapshot: payload exceeds sanity limit";
        return result;
    }

    // Verify declared payload fits within actual data.
    std::size_t payloadOffset = kDeltaHeaderSize;
    if (payloadOffset + payloadBytes > size)
    {
        result.error = "DeltaSnapshot: declared payload exceeds available data";
        return result;
    }

    const std::uint8_t* payload = data + payloadOffset;
    std::size_t         remaining = payloadBytes;
    std::size_t         bytesConsumed = 0;

    // Populate decoded result.
    result.decoded.serverTick     = static_cast<ServerTick>(serverTick);
    result.decoded.baselineTick   = static_cast<ServerTick>(ackSequence);  // ack = baseline.
    result.decoded.sequence       = sequence;
    result.decoded.ackSequence    = ackSequence;
    result.decoded.schemaVersion  = schemaVer;
    result.decoded.entityCount    = entityCount;
    result.decoded.entities.reserve(entityCount);

    // Copy payload for non-owning references.
    result.decoded.payload.assign(payload, payload + payloadBytes);

    // Parse per-entity records.
    for (std::uint32_t e = 0; e < entityCount; ++e)
    {
        // entityId(4) + componentCount(2) = 6 bytes minimum.
        if (remaining < 6)
        {
            result.error = "DeltaSnapshot: truncated entity header at entity " + std::to_string(e);
            return result;
        }

        std::uint32_t entityId = ReadLE32(payload + bytesConsumed);
        std::uint16_t compCount = ReadLE16(payload + bytesConsumed + 4);
        bytesConsumed += 6;
        remaining -= 6;

        EntityDeltaRecord entity;
        entity.entityId = entityId;

        // Tombstone check.
        if (compCount == kWireTombstoneMarker)
        {
            entity.isTombstone = true;
            result.decoded.entities.push_back(std::move(entity));
            continue;
        }

        // Sanity check component count.
        if (compCount > kMaxComponentsPerEntity)
        {
            LOG_WARN(kLogTag,
                     "DeltaSnapshot: component count %u exceeds max %u for entity %u",
                     static_cast<unsigned>(compCount),
                     static_cast<unsigned>(kMaxComponentsPerEntity),
                     static_cast<unsigned>(entityId));
            result.error = "DeltaSnapshot: component count per entity exceeds sanity limit";
            return result;
        }

        entity.isNew = false;
        entity.components.reserve(compCount);

        // Parse per-component records.
        for (std::uint16_t c = 0; c < compCount; ++c)
        {
            // typeId(2) + dataLen(2) = 4 bytes minimum.
            if (remaining < 4)
            {
                result.error = "DeltaSnapshot: truncated component header at entity "
                             + std::to_string(e) + " component " + std::to_string(c);
                return result;
            }

            std::uint16_t typeId  = ReadLE16(payload + bytesConsumed);
            std::uint16_t dataLen = ReadLE16(payload + bytesConsumed + 2);
            bytesConsumed += 4;
            remaining -= 4;

            // Check data fits.
            if (dataLen > remaining)
            {
                LOG_WARN(kLogTag,
                         "DeltaSnapshot: component dataLen %u exceeds remaining %zu for entity %u",
                         static_cast<unsigned>(dataLen),
                         remaining,
                         static_cast<unsigned>(entityId));
                result.error = "DeltaSnapshot: component data exceeds remaining payload";
                return result;
            }

            // Type ID range check.  Skip unknown types (forward-compatible).
            if (typeId > maxTypeId)
            {
                // Skip the data but don't fail.
                bytesConsumed += dataLen;
                remaining -= dataLen;
                continue;
            }

            EntityDeltaRecord::ComponentRef compRef;
            compRef.typeId  = typeId;
            compRef.data    = result.decoded.payload.data() + bytesConsumed;
            compRef.dataLen = dataLen;
            entity.components.push_back(compRef);

            bytesConsumed += dataLen;
            remaining -= dataLen;
        }

        result.decoded.entities.push_back(std::move(entity));
    }

    // Verify we consumed exactly the declared payload.
    if (remaining != 0)
    {
        result.error = "DeltaSnapshot: payload size mismatch (remaining=" + std::to_string(remaining) + ")";
        return result;
    }

    result.success = true;
    return result;
}

} // namespace SagaEngine::Client::Replication
