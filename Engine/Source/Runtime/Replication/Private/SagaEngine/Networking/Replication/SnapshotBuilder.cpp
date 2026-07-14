/// @file SnapshotBuilder.cpp
/// @brief Engine-owned snapshot wire payload builder implementation.

#include "SagaEngine/Networking/Replication/SnapshotBuilder.h"

#include <algorithm>
#include <cstring>

namespace SagaEngine::Networking::Replication {

namespace {

void WriteLE32(std::uint8_t* dst, std::uint32_t value) noexcept
{
    dst[0] = static_cast<std::uint8_t>(value);
    dst[1] = static_cast<std::uint8_t>(value >> 8);
    dst[2] = static_cast<std::uint8_t>(value >> 16);
    dst[3] = static_cast<std::uint8_t>(value >> 24);
}

} // namespace

std::uint32_t ComputeCRC32(const std::uint8_t* data, std::size_t size) noexcept
{
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

SnapshotBuilder::SnapshotBuilder(std::size_t initialPoolCapacity)
    : mPoolCapacity(initialPoolCapacity)
{
    mEntities.reserve(256);
    mDataPool.reserve(initialPoolCapacity);
}

SnapshotBuilder::~SnapshotBuilder() = default;

void SnapshotBuilder::beginSnapshot(std::uint32_t sequenceNumber, std::uint32_t timestamp)
{
    reset();
    mSequenceNumber = sequenceNumber;
    mTimestamp = timestamp;
}

void SnapshotBuilder::addEntity(std::uint32_t entityId, std::uint32_t generation,
                                ComponentMask componentMask,
                                const void* componentData, std::uint32_t totalDataSize)
{
    PooledEntity entity{};
    entity.entityId = entityId;
    entity.generation = generation;
    entity.componentMask = componentMask;
    entity.dirtyMask = componentMask;
    entity.dataOffset = static_cast<std::uint32_t>(mDataPool.size());
    entity.dataSize = totalDataSize;

    if (totalDataSize > 0 && componentData)
    {
        const std::size_t currentSize = mDataPool.size();
        mDataPool.resize(currentSize + totalDataSize);
        std::memcpy(mDataPool.data() + currentSize, componentData, totalDataSize);
    }

    mEntities.push_back(entity);
}

void SnapshotBuilder::markEntityDeleted(std::uint32_t entityId, std::uint32_t generation)
{
    PooledEntity entity{};
    entity.entityId = entityId;
    entity.generation = generation;
    entity.deleted = true;
    mEntities.push_back(entity);
}

std::uint32_t SnapshotBuilder::countDirtyEntities() const
{
    std::uint32_t count = 0;
    for (const auto& e : mEntities)
    {
        if (e.dirtyMask != 0 || e.deleted)
        {
            ++count;
        }
    }
    return count;
}

std::size_t SnapshotBuilder::calculateRequiredSize(bool includeAllEntities) const
{
    std::size_t totalSize = sizeof(WorldSnapshotHeader);
    for (const auto& entity : mEntities)
    {
        const bool shouldInclude = includeAllEntities || entity.dirtyMask != 0 || entity.deleted;
        if (!shouldInclude)
        {
            continue;
        }
        totalSize += includeAllEntities ? 20 : 32;
        totalSize += entity.dataSize;
    }
    return totalSize;
}

std::uint8_t* SnapshotBuilder::finalize()
{
    const std::size_t requiredSize = calculateRequiredSize(true);
    auto* buffer = new std::uint8_t[requiredSize];
    std::memset(buffer, 0, requiredSize);

    auto* header = reinterpret_cast<WorldSnapshotHeader*>(buffer);
    header->magic = WorldSnapshotHeader{}.magic;
    header->version = WorldSnapshotHeader{}.version;
    header->sequenceNumber = mSequenceNumber;
    header->timestamp = mTimestamp;
    header->entityCount = static_cast<std::uint32_t>(mEntities.size());
    header->totalPayloadSize = static_cast<std::uint32_t>(requiredSize - sizeof(WorldSnapshotHeader));

    std::uint8_t* writePtr = buffer + sizeof(WorldSnapshotHeader);
    for (const auto& entity : mEntities)
    {
        WriteLE32(writePtr, entity.entityId); writePtr += 4;
        WriteLE32(writePtr, entity.generation); writePtr += 4;
        std::memcpy(writePtr, &entity.componentMask, 8); writePtr += 8;
        WriteLE32(writePtr, entity.dataSize); writePtr += 4;

        if (!entity.deleted && entity.dataSize > 0)
        {
            std::memcpy(writePtr, mDataPool.data() + entity.dataOffset, entity.dataSize);
            writePtr += entity.dataSize;
        }
    }

    const std::size_t checksumOffset = offsetof(WorldSnapshotHeader, checksum);
    const std::size_t payloadStart = checksumOffset + sizeof(std::uint32_t);
    const std::size_t payloadSize = requiredSize - payloadStart;
    header->checksum = ComputeCRC32(buffer + payloadStart, payloadSize);
    return buffer;
}

std::uint8_t* SnapshotBuilder::buildDelta(std::uint32_t baseSequenceNumber)
{
    const std::uint32_t dirtyCount = countDirtyEntities();
    if (dirtyCount == 0)
    {
        return nullptr;
    }

    const std::size_t requiredSize = calculateRequiredSize(false);
    auto* buffer = new std::uint8_t[requiredSize];
    std::memset(buffer, 0, requiredSize);

    auto* header = reinterpret_cast<DeltaSnapshotHeader*>(buffer);
    header->sequenceNumber = mSequenceNumber;
    header->baseSequenceNumber = baseSequenceNumber;
    header->entityCount = dirtyCount;
    header->totalPayloadSize = static_cast<std::uint32_t>(requiredSize - sizeof(DeltaSnapshotHeader));

    std::uint8_t* writePtr = buffer + sizeof(DeltaSnapshotHeader);
    for (const auto& entity : mEntities)
    {
        if (entity.dirtyMask == 0 && !entity.deleted)
        {
            continue;
        }

        WriteLE32(writePtr, entity.entityId); writePtr += 4;
        WriteLE32(writePtr, entity.generation); writePtr += 4;
        std::memcpy(writePtr, &entity.componentMask, 8); writePtr += 8;
        std::memcpy(writePtr, &entity.dirtyMask, 8); writePtr += 8;
        WriteLE32(writePtr, entity.dataSize); writePtr += 4;
        writePtr[0] = entity.deleted ? 1 : 0; writePtr += 4;

        if (!entity.deleted && entity.dataSize > 0)
        {
            std::memcpy(writePtr, mDataPool.data() + entity.dataOffset, entity.dataSize);
            writePtr += entity.dataSize;
        }
    }

    const std::size_t checksumOffset = offsetof(DeltaSnapshotHeader, checksum);
    const std::size_t payloadStart = checksumOffset + sizeof(std::uint32_t);
    const std::size_t payloadSize = requiredSize - payloadStart;
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

} // namespace SagaEngine::Networking::Replication
