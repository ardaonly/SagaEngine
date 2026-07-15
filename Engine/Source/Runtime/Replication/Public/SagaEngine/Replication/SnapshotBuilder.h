#pragma once

/// @file SnapshotBuilder.h
/// @brief Engine-owned snapshot wire payload builder.

#include "SagaShared/Replication/SnapshotContracts.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <utility>
#include <vector>

namespace SagaEngine::Replication {

using ComponentMask = SagaShared::Replication::ComponentMask;
using WorldSnapshotHeader = SagaShared::Replication::WorldSnapshotHeader;
using DeltaSnapshotHeader = SagaShared::Replication::DeltaSnapshotHeader;

using ComponentSerializeFn = std::function<std::size_t(
    std::uint32_t entityId,
    ComponentMask componentBit,
    std::uint8_t* outBuf,
    std::size_t outBufCapacity)>;

using EntityEnumeratorFn = std::function<std::vector<std::pair<std::uint32_t, ComponentMask>>()>;

class SnapshotBuilder
{
public:
    explicit SnapshotBuilder(std::size_t initialPoolCapacity = 65536);
    ~SnapshotBuilder();

    SnapshotBuilder(const SnapshotBuilder&) = delete;
    SnapshotBuilder& operator=(const SnapshotBuilder&) = delete;
    SnapshotBuilder(SnapshotBuilder&&) = default;
    SnapshotBuilder& operator=(SnapshotBuilder&&) = default;

    void beginSnapshot(std::uint32_t sequenceNumber, std::uint32_t timestamp);
    void addEntity(std::uint32_t entityId, std::uint32_t generation,
                   ComponentMask componentMask,
                   const void* componentData, std::uint32_t totalDataSize);
    void markEntityDeleted(std::uint32_t entityId, std::uint32_t generation);

    [[nodiscard]] std::uint8_t* finalize();
    [[nodiscard]] std::uint8_t* buildDelta(std::uint32_t baseSequenceNumber);
    void reset();

    [[nodiscard]] std::uint32_t sequenceNumber() const noexcept { return mSequenceNumber; }
    [[nodiscard]] std::uint32_t timestamp() const noexcept { return mTimestamp; }
    [[nodiscard]] std::size_t entityCount() const noexcept { return mEntities.size(); }

private:
    struct PooledEntity
    {
        std::uint32_t entityId{0};
        std::uint32_t generation{0};
        ComponentMask componentMask{0};
        ComponentMask dirtyMask{0};
        bool deleted{false};
        std::uint32_t dataOffset{0};
        std::uint32_t dataSize{0};
    };

    [[nodiscard]] std::size_t calculateRequiredSize(bool includeAllEntities) const;
    [[nodiscard]] std::uint32_t countDirtyEntities() const;

    std::vector<PooledEntity> mEntities;
    std::vector<std::uint8_t> mDataPool;
    std::uint32_t mSequenceNumber{0};
    std::uint32_t mTimestamp{0};
    std::size_t mPoolCapacity{0};
};

[[nodiscard]] std::uint32_t ComputeCRC32(const std::uint8_t* data, std::size_t size) noexcept;

} // namespace SagaEngine::Replication
