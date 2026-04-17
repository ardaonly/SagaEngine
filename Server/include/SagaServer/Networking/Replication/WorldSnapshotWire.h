/// @file WorldSnapshotWire.h
/// @brief Server-side snapshot builder for full-world and delta replication.
///
/// Layer  : SagaServer / Networking / Replication
/// Purpose: Production-level snapshot builder that produces binary wire-format
///          payloads for network transmission. Supports:
///            - Full world snapshots (client connect / stale baseline)
///            - Incremental deltas (only dirty entities since client ACK)
///            - Component mask system for selective replication
///            - Cache-friendly contiguous memory layout
///            - Zero-allocation during serialization (pre-allocated pools)
///
/// Wire format (full snapshot):
///   [WorldSnapshotHeader: 32 bytes]
///     magic(4) | version(4) | sequenceNumber(4) | timestamp(4) |
///     entityCount(4) | totalPayloadSize(4) | checksum(4) | reserved(4)
///   [Per Entity:]
///     entityId(4) | generation(4) | componentMask(8) | dataSize(4)
///     [component data: dataSize bytes]
///
/// Wire format (delta):
///   [DeltaSnapshotHeader: 32 bytes]
///     sequenceNumber(4) | baseSequenceNumber(4) | entityCount(4) |
///     totalPayloadSize(4) | checksum(4) | flags(4) | reserved(16)
///   [Per Entity:]
///     entityId(4) | generation(4) | componentMask(8) | dirtyMask(8) |
///     dataSize(4) | deleted(1) | padding(3)
///     [component data: dataSize bytes]

#pragma once

#include "SagaEngine/ECS/Entity.h"
#include "SagaEngine/ECS/Component.h"

#include <cstdint>
#include <cstddef>
#include <vector>
#include <functional>

namespace SagaServer {

// ─── Component type bitmask ──────────────────────────────────────────────────

using ComponentMask = uint64_t;

constexpr ComponentMask COMPONENT_TRANSFORM    = 1ULL << 0;
constexpr ComponentMask COMPONENT_PHYSICS      = 1ULL << 1;
constexpr ComponentMask COMPONENT_RENDER       = 1ULL << 2;
constexpr ComponentMask COMPONENT_ANIMATION    = 1ULL << 3;
constexpr ComponentMask COMPONENT_AUDIO        = 1ULL << 4;
constexpr ComponentMask COMPONENT_NETWORK      = 1ULL << 5;
constexpr ComponentMask COMPONENT_CUSTOM_0     = 1ULL << 6;
constexpr ComponentMask COMPONENT_CUSTOM_1     = 1ULL << 7;
constexpr ComponentMask COMPONENT_CUSTOM_2     = 1ULL << 8;
constexpr ComponentMask COMPONENT_CUSTOM_3     = 1ULL << 9;

// ─── Network EntityId ────────────────────────────────────────────────────────

struct NetEntityId
{
    uint32_t id{0};
    uint32_t generation{0};

    bool operator==(const NetEntityId& o) const { return id == o.id && generation == o.generation; }
    bool operator!=(const NetEntityId& o) const { return !(*this == o); }
    bool operator<(const NetEntityId& o)  const { return id < o.id || (id == o.id && generation < o.generation); }
};

// ─── Per-entity dirty state ──────────────────────────────────────────────────

struct EntityDirtyState
{
    ComponentMask dirtyComponents  {0};
    ComponentMask activeComponents {0};
    uint32_t      lastUpdateTick   {0};
    bool          deleted          {false};
};

// ─── Wire-format headers ─────────────────────────────────────────────────────

struct alignas(8) WorldSnapshotHeader
{
    uint32_t magic             {0x534E4150u}; // "SNAP"
    uint32_t version           {1};
    uint32_t sequenceNumber    {0};
    uint32_t timestamp         {0};
    uint32_t entityCount       {0};
    uint32_t totalPayloadSize  {0};
    uint32_t checksum          {0};
    uint32_t reserved          {0};
};
static_assert(sizeof(WorldSnapshotHeader) == 32, "WorldSnapshotHeader must be 32 bytes");

struct alignas(8) DeltaSnapshotHeader
{
    uint32_t sequenceNumber    {0};
    uint32_t baseSequenceNumber{0};
    uint32_t entityCount       {0};
    uint32_t totalPayloadSize  {0};
    uint32_t checksum          {0};
    uint32_t flags             {0};
    uint8_t  reserved[8]       {};
};
static_assert(sizeof(DeltaSnapshotHeader) == 32, "DeltaSnapshotHeader must be 32 bytes");

// ─── Callbacks ───────────────────────────────────────────────────────────────

/// Serialize one component into outBuf. Returns bytes written, or 0 on failure.
using ComponentSerializeFn = std::function<std::size_t(
    uint32_t          entityId,
    ComponentMask     componentBit,
    uint8_t*          outBuf,
    std::size_t       outBufCapacity)>;

/// Enumerate all replicated entities and their active component masks.
using EntityEnumeratorFn = std::function<
    std::vector<std::pair<uint32_t, ComponentMask>>() >;

// ─── SnapshotBuilder ─────────────────────────────────────────────────────────

class SnapshotBuilder
{
public:
    explicit SnapshotBuilder(std::size_t initialPoolCapacity = 65536);
    ~SnapshotBuilder();

    SnapshotBuilder(const SnapshotBuilder&)            = delete;
    SnapshotBuilder& operator=(const SnapshotBuilder&) = delete;
    SnapshotBuilder(SnapshotBuilder&&)                 = default;
    SnapshotBuilder& operator=(SnapshotBuilder&&)      = default;

    /// Begin building a new snapshot.
    void beginSnapshot(uint32_t sequenceNumber, uint32_t timestamp);

    /// Add an entity with its component data.
    /// componentData must point to contiguous binary blob of all component payloads.
    void addEntity(uint32_t entityId, uint32_t generation,
                   ComponentMask componentMask,
                   const void* componentData, uint32_t totalDataSize);

    /// Mark entity as deleted (tombstone).
    void markEntityDeleted(uint32_t entityId, uint32_t generation);

    /// Finalize and return pointer to wire-format buffer.
    /// Caller takes ownership of the returned buffer (delete[] when done).
    [[nodiscard]] uint8_t* finalize();

    /// Build delta snapshot containing only dirty entities.
    /// baseSequenceNumber is the client's last acknowledged snapshot.
    /// Returns nullptr if no entities are dirty.
    [[nodiscard]] uint8_t* buildDelta(uint32_t baseSequenceNumber);

    /// Reset builder for reuse without deallocating internal pools.
    void reset();

    [[nodiscard]] uint32_t sequenceNumber() const noexcept { return mSequenceNumber; }
    [[nodiscard]] uint32_t timestamp()      const noexcept { return mTimestamp; }
    [[nodiscard]] size_t   entityCount()    const noexcept { return mEntities.size(); }

private:
    struct PooledEntity
    {
        uint32_t        entityId{0};
        uint32_t        generation{0};
        ComponentMask   componentMask{0};
        ComponentMask   dirtyMask{0};
        bool            deleted{false};
        uint32_t        dataOffset{0};
        uint32_t        dataSize{0};
    };

    [[nodiscard]] size_t calculateRequiredSize(bool includeAllEntities) const;
    [[nodiscard]] uint32_t countDirtyEntities() const;

    std::vector<PooledEntity> mEntities;
    std::vector<uint8_t>      mDataPool;
    uint32_t                  mSequenceNumber{0};
    uint32_t                  mTimestamp{0};
    std::size_t               mPoolCapacity{0};
};

// ─── CRC32 ───────────────────────────────────────────────────────────────────

[[nodiscard]] uint32_t ComputeCRC32(const uint8_t* data, std::size_t size) noexcept;

} // namespace SagaServer
