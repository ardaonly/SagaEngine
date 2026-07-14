/// @file WorldSnapshotWire.cpp
/// @brief Engine-side decoder for server-produced full snapshot wire format.

#include "SagaEngine/Client/Replication/WorldSnapshotWire.h"

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

// ─── CRC32 ─────────────────────────────────────────────────────────────────

std::uint32_t ComputeCRC32(const std::uint8_t* data, std::size_t size) noexcept
{
    // IEEE 802.3 polynomial, table-driven implementation.
    static constexpr std::uint32_t kCrcTable[16] = {
        0x00000000, 0x1DB71064, 0x3B6E20C8, 0x26D930AC,
        0x76DC4190, 0x6B6B51F4, 0x4DB26158, 0x5005713C,
        0xEDB88320, 0xF00F9344, 0xD6D6A3E8, 0xCB61B38C,
        0x9B64C2B0, 0x86D3D2D4, 0xA00AE278, 0xBDBDF21C,
    };

    std::uint32_t crc = 0xFFFFFFFFu;
    for (std::size_t i = 0; i < size; ++i)
    {
        crc ^= static_cast<std::uint32_t>(data[i]);
        crc = (crc >> 4) ^ kCrcTable[crc & 0xF];
        crc = (crc >> 4) ^ kCrcTable[crc & 0xF];
    }
    return crc ^ 0xFFFFFFFFu;
}

// ─── Decode ────────────────────────────────────────────────────────────────

SnapshotDecodeResult DecodeSnapshotWire(
    const std::uint8_t* data,
    std::size_t         size,
    std::uint32_t       maxTypeId)
{
    SnapshotDecodeResult result;

    // Bounds check: minimum header size.
    if (size < kExtendedHeaderSize)
    {
        result.error = "WorldSnapshot: payload too small for extended header";
        return result;
    }

    // Parse header fields.
    std::uint32_t magic          = ReadLE32(data + 0);
    std::uint32_t version        = ReadLE32(data + 4);
    std::uint64_t serverTick     = ReadLE64(data + 8);
    std::uint64_t captureTimeMs  = ReadLE64(data + 16);
    std::uint32_t zoneId         = ReadLE32(data + 24);
    std::uint32_t shardId        = ReadLE32(data + 28);
    std::uint32_t entityCount    = ReadLE32(data + 32);
    std::uint32_t payloadCRC32   = ReadLE32(data + 36);
    std::uint64_t byteLength     = ReadLE64(data + 40);
    std::uint32_t schemaVersion  = ReadLE32(data + 48);
    std::uint32_t protocolVer    = ReadLE32(data + 52);

    // Magic check.
    if (magic != kSnapshotMagic)
    {
        result.error = "WorldSnapshot: bad magic 0x" + std::to_string(magic);
        return result;
    }

    // Version check.
    if (version < 2 || version > kSnapshotWireVersion)
    {
        result.error = "WorldSnapshot: unsupported wire version " + std::to_string(version);
        return result;
    }

    // Sanity checks.
    if (entityCount > kMaxSnapshotEntities)
    {
        result.error = "WorldSnapshot: entity count exceeds sanity limit";
        return result;
    }

    if (byteLength > kMaxSnapshotPayloadBytes)
    {
        result.error = "WorldSnapshot: payload size exceeds sanity limit";
        return result;
    }

    // Verify total size.
    if (kExtendedHeaderSize + byteLength > size)
    {
        result.error = "WorldSnapshot: truncated payload (expected "
                     + std::to_string(kExtendedHeaderSize + byteLength)
                     + " bytes, have " + std::to_string(size) + ")";
        return result;
    }

    const std::uint8_t* payload = data + kExtendedHeaderSize;

    // CRC32 check.
    std::uint32_t computedCRC = ComputeCRC32(payload, static_cast<std::size_t>(byteLength));
    if (computedCRC != payloadCRC32)
    {
        LOG_WARN(kLogTag,
                 "WorldSnapshot: CRC32 mismatch (expected 0x%08X, computed 0x%08X)",
                 static_cast<unsigned>(payloadCRC32),
                 static_cast<unsigned>(computedCRC));
        result.error = "WorldSnapshot: CRC32 integrity check failed";
        return result;
    }

    // Populate decoded result.
    result.decoded.serverTick     = static_cast<ServerTick>(serverTick);
    result.decoded.captureTimeMs  = captureTimeMs;
    result.decoded.zoneId         = zoneId;
    result.decoded.shardId        = shardId;
    result.decoded.entityCount    = entityCount;
    result.decoded.schemaVersion  = schemaVersion;
    result.decoded.protocolVersion= protocolVer;
    result.decoded.payloadCRC32   = payloadCRC32;
    result.decoded.entities.reserve(entityCount);

    // Copy payload for non-owning references.
    result.decoded.payload.assign(payload, payload + static_cast<std::size_t>(byteLength));

    // Parse per-entity records.
    std::size_t bytesConsumed = 0;
    std::size_t payloadSize   = static_cast<std::size_t>(byteLength);

    for (std::uint32_t e = 0; e < entityCount; ++e)
    {
        // entityId(4) + componentCount(2) = 6 bytes minimum.
        if (bytesConsumed + 6 > payloadSize)
        {
            result.error = "WorldSnapshot: truncated entity header at entity " + std::to_string(e);
            return result;
        }

        std::uint32_t entityId   = ReadLE32(payload + bytesConsumed);
        std::uint16_t compCount  = ReadLE16(payload + bytesConsumed + 4);
        bytesConsumed += 6;

        SnapshotEntityRecord entity;
        entity.entityId = entityId;
        entity.components.reserve(compCount);

        for (std::uint16_t c = 0; c < compCount; ++c)
        {
            // typeId(2) + dataLen(2) = 4 bytes minimum.
            if (bytesConsumed + 4 > payloadSize)
            {
                result.error = "WorldSnapshot: truncated component at entity "
                             + std::to_string(e) + " component " + std::to_string(c);
                return result;
            }

            std::uint16_t typeId  = ReadLE16(payload + bytesConsumed);
            std::uint16_t dataLen = ReadLE16(payload + bytesConsumed + 2);
            bytesConsumed += 4;

            if (bytesConsumed + dataLen > payloadSize)
            {
                result.error = "WorldSnapshot: component data exceeds payload at entity "
                             + std::to_string(e);
                return result;
            }

            // Type ID range check.
            if (typeId > maxTypeId)
            {
                bytesConsumed += dataLen;
                continue;
            }

            SnapshotEntityRecord::ComponentRef compRef;
            compRef.typeId  = typeId;
            compRef.data    = result.decoded.payload.data() + bytesConsumed;
            compRef.dataLen = dataLen;
            entity.components.push_back(compRef);

            bytesConsumed += dataLen;
        }

        result.decoded.entities.push_back(std::move(entity));
    }

    result.success = true;
    return result;
}

} // namespace SagaEngine::Client::Replication
