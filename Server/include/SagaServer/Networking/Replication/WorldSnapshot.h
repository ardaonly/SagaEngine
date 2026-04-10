/// @file WorldSnapshot.h
/// @brief Full world state capture for checkpoint, reconnect, and replay.
///
/// Layer  : SagaServer / Networking / Replication
/// Purpose: WorldSnapshot serialises the entire replicated entity set into a
///          self-contained binary blob. Used for:
///            - Client reconnect: send full state on rejoin instead of delta history.
///            - Server checkpoint: persist a recoverable world state every N ticks.
///            - Replay: feed captured snapshots back to a deterministic sim.
///
/// Wire format:
///   [SnapshotFileHeader : 72]
///   For each entity:
///     [entityId:4][componentCount:2]
///     For each component: [componentTypeId:2][dataLen:2][data:N]

#pragma once

#include "SagaEngine/ECS/Entity.h"
#include "SagaEngine/ECS/Component.h"
#include "SagaServer/Networking/Replication/ReplicationState.h"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace SagaEngine::Networking::Replication
{

// ─── SnapshotFileHeader ───────────────────────────────────────────────────────

struct alignas(8) SnapshotFileHeader
{
    uint32_t magic{0x53475357};     ///< 'SGSW' — SagaEngine SnapShot World
    uint32_t version{2};
    uint64_t serverTick{0};
    uint64_t captureTimeUnixMs{0};
    uint32_t zoneId{0};
    uint32_t shardId{0};
    uint32_t entityCount{0};
    uint32_t payloadCRC32{0};
    uint64_t byteLength{0};         ///< Payload bytes after the header
    uint8_t  reserved[24]{};
};

static_assert(sizeof(SnapshotFileHeader) == 72, "SnapshotFileHeader must be 72 bytes");

// ─── WorldSnapshotEntityRecord ────────────────────────────────────────────────

struct WorldSnapshotEntityRecord
{
    EntityId                          entityId{0};
    std::vector<ComponentTypeId>      componentTypeIds;
    std::vector<std::vector<uint8_t>> componentData;

    [[nodiscard]] std::size_t ComponentCount() const noexcept
    {
        return componentTypeIds.size();
    }

    void AddComponent(ComponentTypeId typeId, std::vector<uint8_t> data)
    {
        componentTypeIds.push_back(typeId);
        componentData.push_back(std::move(data));
    }

    [[nodiscard]] std::optional<std::size_t> FindComponent(ComponentTypeId typeId) const noexcept
    {
        for (std::size_t i = 0; i < componentTypeIds.size(); ++i)
            if (componentTypeIds[i] == typeId)
                return i;
        return std::nullopt;
    }
};

// ─── WorldSnapshotResult ──────────────────────────────────────────────────────

struct WorldSnapshotResult
{
    bool                  success{false};
    std::vector<uint8_t>  payload;
    std::size_t           entityCount{0};
    std::size_t           byteCount{0};
    uint64_t              durationUs{0};

    [[nodiscard]] bool IsValid() const noexcept { return success && !payload.empty(); }
};

// ─── WorldSnapshotRestoreResult ───────────────────────────────────────────────

struct WorldSnapshotRestoreResult
{
    bool                                   success{false};
    SnapshotFileHeader                     header;
    std::vector<WorldSnapshotEntityRecord> entities;
    std::string                            errorMessage;
};

// ─── Callbacks ────────────────────────────────────────────────────────────────

/// Serialize one component into outBuf. Returns bytes written, or 0 on failure.
using SnapshotSerializeFn = std::function<std::size_t(
    EntityId        entityId,
    ComponentTypeId typeId,
    uint8_t*        outBuf,
    std::size_t     outBufCapacity)>;

/// Apply one component's bytes to the live world. Returns true on success.
using SnapshotDeserializeFn = std::function<bool(
    EntityId        entityId,
    ComponentTypeId typeId,
    const uint8_t*  data,
    std::size_t     size)>;

/// Enumerate all replicated entities and their component type IDs.
using SnapshotEntityEnumeratorFn = std::function<
    std::vector<std::pair<EntityId, std::vector<ComponentTypeId>>>()>;

// ─── WorldSnapshotCapture ─────────────────────────────────────────────────────

class WorldSnapshotCapture final
{
public:
    explicit WorldSnapshotCapture(SnapshotSerializeFn        serializeFn,
                                   SnapshotEntityEnumeratorFn enumeratorFn);

    WorldSnapshotCapture(const WorldSnapshotCapture&)            = delete;
    WorldSnapshotCapture& operator=(const WorldSnapshotCapture&) = delete;

    [[nodiscard]] WorldSnapshotResult Capture(uint64_t serverTick,
                                               uint32_t zoneId  = 0,
                                               uint32_t shardId = 0);

private:
    SnapshotSerializeFn        m_serializeFn;
    SnapshotEntityEnumeratorFn m_enumeratorFn;
    std::vector<uint8_t>       m_scratchBuf;

    static constexpr std::size_t kScratchSize = 65536;
};

// ─── WorldSnapshotDecoder ─────────────────────────────────────────────────────

class WorldSnapshotDecoder final
{
public:
    WorldSnapshotDecoder() = default;

    [[nodiscard]] WorldSnapshotRestoreResult Decode(const uint8_t* data, std::size_t size);

    [[nodiscard]] bool DecodeAndApply(const uint8_t*        data,
                                       std::size_t           size,
                                       SnapshotDeserializeFn deserializeFn);

private:
    [[nodiscard]] bool ValidateHeader(const SnapshotFileHeader& hdr,
                                       std::size_t               totalSize,
                                       std::string&              errorOut) const;
};

} // namespace SagaEngine::Networking::Replication
