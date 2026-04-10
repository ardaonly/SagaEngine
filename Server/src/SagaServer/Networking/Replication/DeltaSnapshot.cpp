/// @file DeltaSnapshot.cpp
/// @brief DeltaSnapshotBuilder and DeltaSnapshotDecoder implementations.

#include "SagaServer/Networking/Replication/DeltaSnapshot.h"
#include "SagaEngine/Core/Log/Log.h"

#include <cassert>
#include <cstring>

namespace SagaEngine::Networking::Replication
{

static constexpr const char* kTag = "DeltaSnapshot";

// ─── LE helpers ───────────────────────────────────────────────────────────────

namespace
{

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

// ─── DeltaSnapshotBuilder ─────────────────────────────────────────────────────

DeltaSnapshotBuilder::DeltaSnapshotBuilder(ComponentSerializeFn serializeFn)
    : m_serializeFn(std::move(serializeFn))
{
    assert(m_serializeFn && "ComponentSerializeFn must not be null");
    m_serializeScratch.resize(kScratchSize);
}

void DeltaSnapshotBuilder::BeginSnapshot(ClientId clientId,
                                          uint64_t serverTick,
                                          uint64_t sequence,
                                          uint64_t clientLastAckedSequence)
{
    m_clientId           = clientId;
    m_header             = DeltaSnapshotHeader{};
    m_header.version     = kDeltaSnapshotVersion;
    m_header.serverTick  = serverTick;
    m_header.sequence    = sequence;
    m_header.ackSequence = clientLastAckedSequence;
    m_pendingDeltas.clear();
}

void DeltaSnapshotBuilder::AddEntity(const EntityDelta& delta)
{
    m_pendingDeltas.push_back(delta);
}

void DeltaSnapshotBuilder::AddTombstone(EntityId entityId)
{
    EntityDelta tombstone;
    tombstone.entityId    = entityId;
    tombstone.isTombstone = true;
    m_pendingDeltas.push_back(std::move(tombstone));
}

void DeltaSnapshotBuilder::PatchHeader(std::vector<uint8_t>& buf,
                                        uint32_t entityCount) const
{
    assert(buf.size() >= kHeaderWireSize);

    const uint32_t payloadBytes =
        static_cast<uint32_t>(buf.size() - kHeaderWireSize);

    std::size_t off = 0;
    std::memcpy(buf.data() + off, &m_header.version,     4); off += 4;
    std::memcpy(buf.data() + off, &m_header.serverTick,  8); off += 8;
    std::memcpy(buf.data() + off, &m_header.sequence,    8); off += 8;
    std::memcpy(buf.data() + off, &m_header.ackSequence, 8); off += 8;
    std::memcpy(buf.data() + off, &entityCount,          4); off += 4;
    std::memcpy(buf.data() + off, &payloadBytes,         4);
}

void DeltaSnapshotBuilder::WriteEntityDelta(std::vector<uint8_t>& buf,
                                              const EntityDelta&    delta) const
{
    WriteLE(buf, delta.entityId);

    if (delta.isTombstone)
    {
        WriteLE(buf, kTombstoneComponentCount);
        return;
    }

    WriteLE(buf, static_cast<uint16_t>(delta.components.size()));

    for (const auto& comp : delta.components)
    {
        WriteLE(buf, static_cast<uint16_t>(comp.typeId));
        WriteLE(buf, static_cast<uint16_t>(comp.bytes.size()));
        buf.insert(buf.end(), comp.bytes.begin(), comp.bytes.end());
    }
}

std::vector<std::vector<uint8_t>> DeltaSnapshotBuilder::Finalize()
{
    std::vector<std::vector<uint8_t>> chunks;

    if (m_pendingDeltas.empty())
        return chunks;

    std::vector<uint8_t> current;
    current.reserve(kMaxSnapshotBytes);
    current.resize(kHeaderWireSize, 0);

    uint32_t entityCountInChunk = 0;

    for (const auto& delta : m_pendingDeltas)
    {
        const std::size_t estimatedSize = delta.EstimatedWireBytes();

        if (entityCountInChunk > 0 &&
            current.size() + estimatedSize > kMaxSnapshotBytes)
        {
            PatchHeader(current, entityCountInChunk);
            chunks.push_back(std::move(current));

            current.clear();
            current.resize(kHeaderWireSize, 0);
            entityCountInChunk = 0;
        }

        WriteEntityDelta(current, delta);
        ++entityCountInChunk;
    }

    if (entityCountInChunk > 0)
    {
        PatchHeader(current, entityCountInChunk);
        chunks.push_back(std::move(current));
    }

    LOG_DEBUG(kTag,
        "Client %llu: built %zu chunk(s) covering %zu entities — seq %llu",
        static_cast<unsigned long long>(m_clientId),
        chunks.size(),
        m_pendingDeltas.size(),
        static_cast<unsigned long long>(m_header.sequence));

    return chunks;
}

void DeltaSnapshotBuilder::Reset()
{
    m_pendingDeltas.clear();
    m_clientId = 0;
    m_header   = {};
}

// ─── DeltaSnapshotDecoder ─────────────────────────────────────────────────────

bool DeltaSnapshotDecoder::Decode(const uint8_t* data, std::size_t size)
{
    m_deltas.clear();
    m_header = {};

    if (!data || size < DeltaSnapshotBuilder::kHeaderWireSize)
    {
        LOG_WARN(kTag, "Decode: payload too short (%zu bytes)", size);
        return false;
    }

    std::size_t offset = 0;

    if (!ReadLE(data, size, offset, m_header.version))      return false;
    if (!ReadLE(data, size, offset, m_header.serverTick))   return false;
    if (!ReadLE(data, size, offset, m_header.sequence))     return false;
    if (!ReadLE(data, size, offset, m_header.ackSequence))  return false;
    if (!ReadLE(data, size, offset, m_header.entityCount))  return false;
    if (!ReadLE(data, size, offset, m_header.payloadBytes)) return false;

    if (m_header.version != kDeltaSnapshotVersion)
    {
        LOG_WARN(kTag, "Decode: unsupported snapshot version %u", m_header.version);
        return false;
    }

    m_deltas.reserve(m_header.entityCount);

    for (uint32_t e = 0; e < m_header.entityCount; ++e)
    {
        EntityDelta delta;

        if (!ReadLE(data, size, offset, delta.entityId))
        {
            LOG_WARN(kTag, "Decode: unexpected end reading entityId at entity %u", e);
            return false;
        }

        uint16_t componentCount = 0;
        if (!ReadLE(data, size, offset, componentCount))
        {
            LOG_WARN(kTag, "Decode: unexpected end reading componentCount at entity %u", e);
            return false;
        }

        if (componentCount == kTombstoneComponentCount)
        {
            delta.isTombstone = true;
            m_deltas.push_back(std::move(delta));
            continue;
        }

        delta.components.reserve(componentCount);

        for (uint16_t c = 0; c < componentCount; ++c)
        {
            ComponentData comp;
            uint16_t typeId = 0;
            uint16_t dataLen = 0;

            if (!ReadLE(data, size, offset, typeId) ||
                !ReadLE(data, size, offset, dataLen))
            {
                LOG_WARN(kTag,
                    "Decode: unexpected end reading component header "
                    "(entity %u, component %u)",
                    e, c);
                return false;
            }

            comp.typeId = static_cast<ComponentTypeId>(typeId);

            if (offset + dataLen > size)
            {
                LOG_WARN(kTag,
                    "Decode: component data overflows payload "
                    "(entity %llu, component type %u, dataLen %u)",
                    static_cast<unsigned long long>(delta.entityId),
                    static_cast<unsigned int>(typeId),
                    static_cast<unsigned int>(dataLen));
                return false;
            }

            comp.bytes.assign(data + offset, data + offset + dataLen);
            offset += dataLen;

            delta.components.push_back(std::move(comp));
        }

        m_deltas.push_back(std::move(delta));
    }

    return true;
}

const DeltaSnapshotHeader& DeltaSnapshotDecoder::GetHeader() const noexcept
{
    return m_header;
}

const std::vector<EntityDelta>& DeltaSnapshotDecoder::GetDeltas() const noexcept
{
    return m_deltas;
}

void DeltaSnapshotDecoder::Clear()
{
    m_header = {};
    m_deltas.clear();
}

} // namespace SagaEngine::Networking::Replication
