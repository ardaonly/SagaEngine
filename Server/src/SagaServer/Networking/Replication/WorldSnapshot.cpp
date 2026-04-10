/// @file WorldSnapshot.cpp
/// @brief WorldSnapshotCapture and WorldSnapshotDecoder implementations.

#include "SagaServer/Networking/Replication/WorldSnapshot.h"
#include "SagaEngine/Core/Log/Log.h"

#include <cassert>
#include <chrono>
#include <cstring>

namespace SagaEngine::Networking::Replication
{

static constexpr const char* kTag = "WorldSnapshot";

// ─── CRC32 ────────────────────────────────────────────────────────────────────

namespace
{

uint32_t ComputeCRC32(const uint8_t* data, std::size_t size) noexcept
{
    uint32_t crc = 0xFFFFFFFFu;
    for (std::size_t i = 0; i < size; ++i)
    {
        crc ^= data[i];
        for (int bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ (0xEDB88320u & -(crc & 1u));
    }
    return crc ^ 0xFFFFFFFFu;
}

template<typename T>
void WriteLE(std::vector<uint8_t>& buf, T value) noexcept
{
    static_assert(std::is_trivially_copyable_v<T>);
    const auto bytes = reinterpret_cast<const uint8_t*>(&value);
    buf.insert(buf.end(), bytes, bytes + sizeof(T));
}

template<typename T>
bool ReadLE(const uint8_t* data, std::size_t size, std::size_t& offset, T& out) noexcept
{
    if (offset + sizeof(T) > size)
        return false;
    std::memcpy(&out, data + offset, sizeof(T));
    offset += sizeof(T);
    return true;
}

} // anonymous namespace

// ─── WorldSnapshotCapture ─────────────────────────────────────────────────────

WorldSnapshotCapture::WorldSnapshotCapture(SnapshotSerializeFn        serializeFn,
                                             SnapshotEntityEnumeratorFn enumeratorFn)
    : m_serializeFn(std::move(serializeFn))
    , m_enumeratorFn(std::move(enumeratorFn))
{
    assert(m_serializeFn  && "SnapshotSerializeFn must not be null");
    assert(m_enumeratorFn && "SnapshotEntityEnumeratorFn must not be null");
    m_scratchBuf.resize(kScratchSize);
}

WorldSnapshotResult WorldSnapshotCapture::Capture(uint64_t serverTick,
                                                    uint32_t zoneId,
                                                    uint32_t shardId)
{
    const auto captureStart = std::chrono::steady_clock::now();

    WorldSnapshotResult result;

    const auto entityList = m_enumeratorFn();

    if (entityList.empty())
    {
        result.success     = true;
        result.entityCount = 0;
        LOG_INFO(kTag, "Snapshot captured — tick %llu | 0 entities (world empty)",
                 static_cast<unsigned long long>(serverTick));
        return result;
    }

    std::vector<uint8_t> payload;
    payload.reserve(entityList.size() * 256);
    payload.resize(sizeof(SnapshotFileHeader), 0);

    uint32_t entityCount = 0;

    for (const auto& [entityId, componentTypes] : entityList)
    {
        if (componentTypes.empty())
            continue;

        WriteLE(payload, entityId);
        WriteLE(payload, static_cast<uint16_t>(componentTypes.size()));

        for (ComponentTypeId typeId : componentTypes)
        {
            const std::size_t written = m_serializeFn(
                entityId, typeId,
                m_scratchBuf.data(), m_scratchBuf.size());

            if (written == 0)
            {
                LOG_WARN(kTag,
                    "Serialize returned 0 for entity %llu component %u — skipping",
                    static_cast<unsigned long long>(entityId),
                    static_cast<unsigned int>(typeId));
            }

            WriteLE(payload, static_cast<uint16_t>(typeId));
            WriteLE(payload, static_cast<uint16_t>(written));

            if (written > 0)
                payload.insert(payload.end(),
                               m_scratchBuf.data(),
                               m_scratchBuf.data() + written);
        }

        ++entityCount;
    }

    const std::size_t payloadOffset = sizeof(SnapshotFileHeader);
    const uint32_t crc = ComputeCRC32(
        payload.data() + payloadOffset,
        payload.size() - payloadOffset);

    SnapshotFileHeader hdr;
    hdr.serverTick = serverTick;
    hdr.captureTimeUnixMs = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
    hdr.zoneId       = zoneId;
    hdr.shardId      = shardId;
    hdr.entityCount  = entityCount;
    hdr.payloadCRC32 = crc;
    hdr.byteLength   = static_cast<uint64_t>(payload.size() - payloadOffset);

    std::memcpy(payload.data(), &hdr, sizeof(hdr));

    const auto captureEnd = std::chrono::steady_clock::now();

    result.success     = true;
    result.payload     = std::move(payload);
    result.entityCount = entityCount;
    result.byteCount   = result.payload.size();
    result.durationUs  = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(
            captureEnd - captureStart).count());

    LOG_INFO(kTag,
        "Snapshot captured — tick %llu | entities %zu | bytes %zu | crc 0x%08X | %llu us",
        static_cast<unsigned long long>(serverTick),
        result.entityCount,
        result.byteCount,
        crc,
        static_cast<unsigned long long>(result.durationUs));

    return result;
}

// ─── WorldSnapshotDecoder ─────────────────────────────────────────────────────

WorldSnapshotRestoreResult WorldSnapshotDecoder::Decode(const uint8_t* data, std::size_t size)
{
    WorldSnapshotRestoreResult result;

    if (!data || size < sizeof(SnapshotFileHeader))
    {
        result.errorMessage = "Payload too small to contain a valid header";
        return result;
    }

    std::memcpy(&result.header, data, sizeof(SnapshotFileHeader));

    if (!ValidateHeader(result.header, size, result.errorMessage))
        return result;

    std::size_t offset = sizeof(SnapshotFileHeader);
    result.entities.reserve(result.header.entityCount);

    for (uint32_t e = 0; e < result.header.entityCount; ++e)
    {
        WorldSnapshotEntityRecord rec;
        uint16_t componentCount = 0;

        if (!ReadLE(data, size, offset, rec.entityId) ||
            !ReadLE(data, size, offset, componentCount))
        {
            result.errorMessage = "Unexpected end of data reading entity header at index "
                + std::to_string(e);
            return result;
        }

        rec.componentTypeIds.reserve(componentCount);
        rec.componentData.reserve(componentCount);

        for (uint16_t c = 0; c < componentCount; ++c)
        {
            uint16_t typeId  = 0;
            uint16_t dataLen = 0;

            if (!ReadLE(data, size, offset, typeId) ||
                !ReadLE(data, size, offset, dataLen))
            {
                result.errorMessage = "Unexpected end of data reading component header "
                    "(entity index " + std::to_string(e) + ", component " + std::to_string(c) + ")";
                return result;
            }

            if (offset + dataLen > size)
            {
                result.errorMessage = "Component data overflows payload "
                    "(entity index " + std::to_string(e) + ")";
                return result;
            }

            std::vector<uint8_t> compData(data + offset, data + offset + dataLen);
            offset += dataLen;

            rec.componentTypeIds.push_back(static_cast<ComponentTypeId>(typeId));
            rec.componentData.push_back(std::move(compData));
        }

        result.entities.push_back(std::move(rec));
    }

    result.success = true;

    LOG_INFO(kTag, "Snapshot decoded — tick %llu | entities %zu",
             static_cast<unsigned long long>(result.header.serverTick),
             result.entities.size());

    return result;
}

bool WorldSnapshotDecoder::DecodeAndApply(const uint8_t*        data,
                                           std::size_t           size,
                                           SnapshotDeserializeFn deserializeFn)
{
    auto decoded = Decode(data, size);
    if (!decoded.success)
    {
        LOG_ERROR(kTag, "DecodeAndApply: decode failed — %s",
                  decoded.errorMessage.c_str());
        return false;
    }

    uint32_t applied = 0;
    uint32_t failed  = 0;

    for (const auto& rec : decoded.entities)
    {
        for (std::size_t c = 0; c < rec.componentTypeIds.size(); ++c)
        {
            const bool ok = deserializeFn(
                rec.entityId,
                rec.componentTypeIds[c],
                rec.componentData[c].data(),
                rec.componentData[c].size());

            ok ? ++applied : ++failed;
        }
    }

    LOG_INFO(kTag,
        "DecodeAndApply: applied %u components (%u failed) across %zu entities",
        applied, failed, decoded.entities.size());

    return failed == 0;
}

bool WorldSnapshotDecoder::ValidateHeader(const SnapshotFileHeader& hdr,
                                           std::size_t               totalSize,
                                           std::string&              errorOut) const
{
    if (hdr.magic != 0x53475357u)
    {
        errorOut = "Bad magic number — not a SagaEngine world snapshot";
        return false;
    }

    if (hdr.version != 2)
    {
        errorOut = "Unsupported snapshot version " + std::to_string(hdr.version);
        return false;
    }

    const std::size_t payloadOffset = sizeof(SnapshotFileHeader);
    const std::size_t payloadSize   = totalSize - payloadOffset;

    if (static_cast<uint64_t>(payloadSize) != hdr.byteLength)
    {
        errorOut = "Payload length mismatch: header declares "
            + std::to_string(hdr.byteLength) + " bytes but buffer contains "
            + std::to_string(payloadSize);
        return false;
    }

    const uint32_t actualCRC = ComputeCRC32(
        reinterpret_cast<const uint8_t*>(&hdr) + payloadOffset,
        payloadSize);

    if (actualCRC != hdr.payloadCRC32)
    {
        errorOut = "CRC32 mismatch — snapshot may be corrupt";
        return false;
    }

    return true;
}

} // namespace SagaEngine::Networking::Replication
