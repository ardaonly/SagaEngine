/// @file DeltaSnapshot.h
/// @brief Component-level delta diff computation and wire encoding.
///
/// Layer  : SagaServer / Networking / Replication
/// Purpose: DeltaSnapshot computes the minimum set of component bytes that
///          must be transmitted to bring a specific client's understanding of
///          the world from its ACKed baseline to the current server state.
///
/// Wire format per entity in a DeltaSnapshot packet:
///   [entityId:4][componentCount:2]
///   For each component:
///     [componentTypeId:2][dataLength:2][data:N]
///
/// Guarantees:
///   - Components not marked dirty since baseline are omitted.
///   - New entities always receive a full component dump regardless of dirty state.
///   - Destroyed entities emit a tombstone record (componentCount=0xFFFF).
///   - If the payload exceeds kMaxSnapshotBytes the builder splits it into
///     multiple MTU-safe chunks, each with its own DeltaSnapshotHeader.

#pragma once

#include "SagaEngine/ECS/Entity.h"
#include "SagaEngine/ECS/Component.h"
#include "SagaServer/Networking/Replication/ReplicationState.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>

namespace SagaEngine::Networking::Replication
{

// ─── Constants ────────────────────────────────────────────────────────────────

static constexpr uint16_t    kTombstoneComponentCount = 0xFFFF;
static constexpr uint32_t    kDeltaSnapshotVersion    = 1;
static constexpr std::size_t kMaxSnapshotBytes        = 1200;   ///< MTU-safe payload cap

// ─── ComponentData ────────────────────────────────────────────────────────────

struct ComponentData
{
    ComponentTypeId      typeId{0};
    std::vector<uint8_t> bytes;
};

// ─── EntityDelta ──────────────────────────────────────────────────────────────

struct EntityDelta
{
    EntityId                    entityId{0};
    bool                        isTombstone{false};
    bool                        isNew{false};
    std::vector<ComponentData>  components;

    [[nodiscard]] std::size_t EstimatedWireBytes() const noexcept
    {
        std::size_t total = 6; // entityId(4) + componentCount(2)
        for (const auto& c : components)
            total += 4 + c.bytes.size(); // typeId(2) + len(2) + data
        return total;
    }
};

// ─── DeltaSnapshotHeader ──────────────────────────────────────────────────────

struct DeltaSnapshotHeader
{
    uint32_t version{kDeltaSnapshotVersion};
    uint64_t serverTick{0};
    uint64_t sequence{0};
    uint64_t ackSequence{0};
    uint32_t entityCount{0};
    uint32_t payloadBytes{0};
};

// ─── ComponentSerializeFn ─────────────────────────────────────────────────────

/// Serialize one component into outBuf. Returns bytes written, or 0 on failure.
using ComponentSerializeFn = std::function<std::size_t(
    EntityId        entityId,
    ComponentTypeId typeId,
    uint8_t*        outBuf,
    std::size_t     outBufSize)>;

// ─── DeltaSnapshotBuilder ─────────────────────────────────────────────────────

class DeltaSnapshotBuilder final
{
public:
    explicit DeltaSnapshotBuilder(ComponentSerializeFn serializeFn);

    DeltaSnapshotBuilder(const DeltaSnapshotBuilder&)            = delete;
    DeltaSnapshotBuilder& operator=(const DeltaSnapshotBuilder&) = delete;

    void BeginSnapshot(ClientId clientId,
                       uint64_t serverTick,
                       uint64_t sequence,
                       uint64_t clientLastAckedSequence);

    void AddEntity(const EntityDelta& delta);
    void AddTombstone(EntityId entityId);

    /// Finalise and return MTU-split chunks. Each chunk carries a full header.
    [[nodiscard]] std::vector<std::vector<uint8_t>> Finalize();

    void Reset();

    /// Wire size of the snapshot header (used by Decode).
    static constexpr std::size_t kHeaderWireSize =
        sizeof(uint32_t) +  // version
        sizeof(uint64_t) +  // serverTick
        sizeof(uint64_t) +  // sequence
        sizeof(uint64_t) +  // ackSequence
        sizeof(uint32_t) +  // entityCount
        sizeof(uint32_t);   // payloadBytes

private:
    void PatchHeader(std::vector<uint8_t>& buf, uint32_t entityCount) const;
    void WriteEntityDelta(std::vector<uint8_t>& buf, const EntityDelta& delta) const;

    ComponentSerializeFn     m_serializeFn;
    ClientId                 m_clientId{0};
    DeltaSnapshotHeader      m_header;
    std::vector<EntityDelta> m_pendingDeltas;

    static constexpr std::size_t kScratchSize = 8192;
    std::vector<uint8_t>         m_serializeScratch;
};

// ─── DeltaSnapshotDecoder ─────────────────────────────────────────────────────

class DeltaSnapshotDecoder final
{
public:
    DeltaSnapshotDecoder() = default;

    /// Decode a raw payload. Returns false if the payload is malformed.
    [[nodiscard]] bool Decode(const uint8_t* data, std::size_t size);

    [[nodiscard]] const DeltaSnapshotHeader&      GetHeader() const noexcept;
    [[nodiscard]] const std::vector<EntityDelta>& GetDeltas() const noexcept;

    void Clear();

private:
    DeltaSnapshotHeader      m_header;
    std::vector<EntityDelta> m_deltas;
};

} // namespace SagaEngine::Networking::Replication
