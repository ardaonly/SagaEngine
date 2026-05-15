#pragma once

/// @file SnapshotContracts.hpp
/// @brief Data-only replication wire contracts shared by engine and server.

#include <cstdint>

namespace SagaShared::Replication {

using ComponentMask = std::uint64_t;

inline constexpr ComponentMask COMPONENT_TRANSFORM = 1ULL << 0;
inline constexpr ComponentMask COMPONENT_PHYSICS = 1ULL << 1;
inline constexpr ComponentMask COMPONENT_RENDER = 1ULL << 2;
inline constexpr ComponentMask COMPONENT_ANIMATION = 1ULL << 3;
inline constexpr ComponentMask COMPONENT_AUDIO = 1ULL << 4;
inline constexpr ComponentMask COMPONENT_NETWORK = 1ULL << 5;
inline constexpr ComponentMask COMPONENT_CUSTOM_0 = 1ULL << 6;
inline constexpr ComponentMask COMPONENT_CUSTOM_1 = 1ULL << 7;
inline constexpr ComponentMask COMPONENT_CUSTOM_2 = 1ULL << 8;
inline constexpr ComponentMask COMPONENT_CUSTOM_3 = 1ULL << 9;

struct NetEntityId
{
    std::uint32_t id{0};
    std::uint32_t generation{0};

    [[nodiscard]] bool operator==(const NetEntityId& o) const
    {
        return id == o.id && generation == o.generation;
    }

    [[nodiscard]] bool operator!=(const NetEntityId& o) const
    {
        return !(*this == o);
    }

    [[nodiscard]] bool operator<(const NetEntityId& o) const
    {
        return id < o.id || (id == o.id && generation < o.generation);
    }
};

struct EntityDirtyState
{
    ComponentMask dirtyComponents{0};
    ComponentMask activeComponents{0};
    std::uint32_t lastUpdateTick{0};
    bool deleted{false};
};

struct alignas(8) WorldSnapshotHeader
{
    std::uint32_t magic{0x534E4150u};
    std::uint32_t version{1};
    std::uint32_t sequenceNumber{0};
    std::uint32_t timestamp{0};
    std::uint32_t entityCount{0};
    std::uint32_t totalPayloadSize{0};
    std::uint32_t checksum{0};
    std::uint32_t reserved{0};
};
static_assert(sizeof(WorldSnapshotHeader) == 32, "WorldSnapshotHeader must be 32 bytes");

struct alignas(8) DeltaSnapshotHeader
{
    std::uint32_t sequenceNumber{0};
    std::uint32_t baseSequenceNumber{0};
    std::uint32_t entityCount{0};
    std::uint32_t totalPayloadSize{0};
    std::uint32_t checksum{0};
    std::uint32_t flags{0};
    std::uint8_t reserved[8]{};
};
static_assert(sizeof(DeltaSnapshotHeader) == 32, "DeltaSnapshotHeader must be 32 bytes");

} // namespace SagaShared::Replication
