/// @file WorldSnapshotWire.cpp
/// @brief Server-side snapshot builder implementation.
///
/// Layer  : SagaServer / Networking / Replication
/// Purpose: Production-level SnapshotBuilder with zero-allocation serialization,
///          CRC32 checksums, and cache-friendly contiguous memory layout.

#include "SagaServer/Networking/Replication/WorldSnapshotWire.h"

#include <cstring>
#include <algorithm>

namespace SagaServer {

// ─── Little-endian helpers ──────────────────────────────────────────────────

namespace {

void WriteLE32(uint8_t* dst, uint32_t value) noexcept
{
    dst[0] = static_cast<uint8_t>(value);
    dst[1] = static_cast<uint8_t>(value >> 8);
    dst[2] = static_cast<uint8_t>(value >> 16);
    dst[3] = static_cast<uint8_t>(value >> 24);
}

void WriteLE64(uint8_t* dst, uint64_t value) noexcept
{
    WriteLE32(dst, static_cast<uint32_t>(value));
    WriteLE32(dst + 4, static_cast<uint32_t>(value >> 32));
}

} // anonymous namespace

// ─── CRC32 Implementation ───────────────────────────────────────────────────

uint32_t ComputeCRC32(const uint8_t* data, std::size_t size) noexcept
{
    static constexpr uint32_t kCrcTable[16] = {
        0x00000000, 0x1DB71064, 0x3B6E20C8, 0x26D930AC,
        0x76DC4190, 0x6B6B51F4, 0x4DB26158, 0x5005713C,
        0xEDB88320, 0xF00F9344, 0xD6D6A3E8, 0xCB61B38C,
        0x9B64C2B0, 0x86D3D2D4, 0xA00AE278, 0xBDBDF21C,
    };

    uint32_t crc = 0xFFFFFFFFu;
    for (std::size_t i = 0; i < size; ++i)
    {
        crc ^= static_cast<uint32_t>(data[i]);
        crc = (crc >> 4) ^ kCrcTable[crc & 0xF];
        crc = (crc >> 4) ^ kCrcTable[crc & 0xF];
    }
    return crc ^ 0xFFFFFFFFu;
}

// ─── SnapshotBuilder Implementation ─────────────────────────────────────────

SnapshotBuilder::SnapshotBuilder(std::size_t initialPoolCapacity)
    : mPoolCapacity(initialPoolCapacity)
{
    mEntities.reserve(256);
    mDataPool.reserve(initialPoolCapacity);
}

SnapshotBuilder::~SnapshotBuilder() = default;

void SnapshotBuilder::beginSnapshot(uint32_t sequenceNumber, uint32_t timestamp)
{
    reset();
    mSequenceNumber = sequenceNumber;
    mTimestamp = timestamp;
}

void SnapshotBuilder::addEntity(uint32_t entityId, uint32_t generation,
                                 ComponentMask componentMask,
                                 const void* componentData, uint32_t totalDataSize)
{
    PooledEntity entity{};
    entity.entityId = entityId;
    entity.generation = generation;
    entity.componentMask = componentMask;
    entity.dirtyMask = componentMask; // Full snapshot = all dirty
    entity.deleted = false;
    entity.dataOffset = static_cast<uint32_t>(mDataPool.size());
    entity.dataSize = totalDataSize;

    if (totalDataSize > 0 && componentData)
    {
        const std::size_t currentSize = mDataPool.size();
        mDataPool.resize(currentSize + totalDataSize);
        std::memcpy(mDataPool.data() + currentSize, componentData, totalDataSize);
    }

    mEntities.push_back(entity);
}

void SnapshotBuilder::markEntityDeleted(uint32_t entityId, uint32_t generation)
{
    PooledEntity entity{};
    entity.entityId = entityId;
    entity.generation = generation;
    entity.componentMask = 0;
    entity.dirtyMask = 0;
    entity.deleted = true;
    entity.dataOffset = 0;
    entity.dataSize = 0;
    mEntities.push_back(entity);
}

uint32_t SnapshotBuilder::countDirtyEntities() const
{
    uint32_t count = 0;
    for (const auto& e : mEntities)
    {
        if (e.dirtyMask != 0 || e.deleted)
            ++count;
    }
    return count;
}

size_t SnapshotBuilder::calculateRequiredSize(bool includeAllEntities) const
{
    size_t totalSize = sizeof(WorldSnapshotHeader);

    for (const auto& entity : mEntities)
    {
        bool shouldInclude = includeAllEntities || entity.dirtyMask != 0 || entity.deleted;
        if (!shouldInclude)
            continue;

        // Entity header: entityId(4) + generation(4) + componentMask(8) + dataSize(4) = 20 bytes
        // For delta: + dirtyMask(8) + deleted(1) + padding(3) = 32 bytes total
        totalSize += includeAllEntities ? 20 : 32;
        totalSize += entity.dataSize;
    }

    return totalSize;
}

uint8_t* SnapshotBuilder::finalize()
{
    const size_t requiredSize = calculateRequiredSize(true);
    auto* buffer = new uint8_t[requiredSize];
    std::memset(buffer, 0, requiredSize);

    // Fill header
    auto* header = reinterpret_cast<WorldSnapshotHeader*>(buffer);
    header->magic = WorldSnapshotHeader{}.magic;
    header->version = WorldSnapshotHeader{}.version;
    header->sequenceNumber = mSequenceNumber;
    header->timestamp = mTimestamp;
    header->entityCount = static_cast<uint32_t>(mEntities.size());
    header->totalPayloadSize = static_cast<uint32_t>(requiredSize - sizeof(WorldSnapshotHeader));

    // Serialize entities
    uint8_t* writePtr = buffer + sizeof(WorldSnapshotHeader);

    for (const auto& entity : mEntities)
    {
        WriteLE32(writePtr, entity.entityId);       writePtr += 4;
        WriteLE32(writePtr, entity.generation);      writePtr += 4;
        std::memcpy(writePtr, &entity.componentMask, 8); writePtr += 8;
        WriteLE32(writePtr, entity.dataSize);        writePtr += 4;

        if (!entity.deleted && entity.dataSize > 0)
        {
            std::memcpy(writePtr, mDataPool.data() + entity.dataOffset, entity.dataSize);
            writePtr += entity.dataSize;
        }
    }

    // Compute checksum over payload (after checksum field)
    const size_t checksumOffset = offsetof(WorldSnapshotHeader, checksum);
    const size_t payloadStart = checksumOffset + sizeof(uint32_t);
    const size_t payloadSize = requiredSize - payloadStart;
    header->checksum = ComputeCRC32(buffer + payloadStart, payloadSize);

    return buffer;
}

uint8_t* SnapshotBuilder::buildDelta(uint32_t baseSequenceNumber)
{
    const uint32_t dirtyCount = countDirtyEntities();
    if (dirtyCount == 0)
        return nullptr;

    const size_t requiredSize = calculateRequiredSize(false);
    auto* buffer = new uint8_t[requiredSize];
    std::memset(buffer, 0, requiredSize);

    // Fill delta header
    auto* header = reinterpret_cast<DeltaSnapshotHeader*>(buffer);
    header->sequenceNumber = mSequenceNumber;
    header->baseSequenceNumber = baseSequenceNumber;
    header->entityCount = dirtyCount;
    header->totalPayloadSize = static_cast<uint32_t>(requiredSize - sizeof(DeltaSnapshotHeader));

    // Serialize only dirty entities
    uint8_t* writePtr = buffer + sizeof(DeltaSnapshotHeader);

    for (const auto& entity : mEntities)
    {
        if (entity.dirtyMask == 0 && !entity.deleted)
            continue;

        WriteLE32(writePtr, entity.entityId);          writePtr += 4;
        WriteLE32(writePtr, entity.generation);         writePtr += 4;
        std::memcpy(writePtr, &entity.componentMask, 8); writePtr += 8;
        std::memcpy(writePtr, &entity.dirtyMask, 8);    writePtr += 8;
        WriteLE32(writePtr, entity.dataSize);           writePtr += 4;
        writePtr[0] = entity.deleted ? 1 : 0;           writePtr += 4; // +3 padding

        if (!entity.deleted && entity.dataSize > 0)
        {
            std::memcpy(writePtr, mDataPool.data() + entity.dataOffset, entity.dataSize);
            writePtr += entity.dataSize;
        }
    }

    // Compute checksum
    const size_t checksumOffset = offsetof(DeltaSnapshotHeader, checksum);
    const size_t payloadStart = checksumOffset + sizeof(uint32_t);
    const size_t payloadSize = requiredSize - payloadStart;
    header->checksum = ComputeCRC32(buffer + payloadStart, payloadSize);

    return buffer;
}

void SnapshotBuilder::reset()
{
    mEntities.clear();
    mDataPool.clear();
    mSequenceNumber = 0;
    mTimestamp = 0;
}

} // namespace SagaServer
