/// @file StateSnapshot.hpp
/// @brief Deterministic state snapshot comparison contracts.

#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>

namespace SagaStateCheck
{

inline constexpr std::uint32_t kStateSnapshotSchemaVersion = 1;

struct StateVector3
{
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

struct EntitySnapshot
{
    std::uint64_t entityId = 0;
    std::uint64_t ownerClientId = 0;
    StateVector3 position;
    bool hasVelocity = false;
    StateVector3 velocity;
};

struct StateSnapshot
{
    std::uint32_t schemaVersion = kStateSnapshotSchemaVersion;
    std::string snapshotId;
    std::uint64_t tick = 0;
    std::vector<EntitySnapshot> entities;
};

struct EntityChecksum
{
    std::uint64_t value = 0;
};

enum class DesyncStatus
{
    Passed = 0,
    Failed,
};

struct OwnershipMismatch
{
    std::uint64_t entityId = 0;
    std::uint64_t expectedOwnerClientId = 0;
    std::uint64_t actualOwnerClientId = 0;
};

struct PositionMismatch
{
    std::uint64_t entityId = 0;
    StateVector3 expectedPosition;
    StateVector3 actualPosition;
};

struct ChecksumMismatch
{
    std::uint64_t entityId = 0;
    EntityChecksum expectedChecksum;
    EntityChecksum actualChecksum;
};

struct DesyncReport
{
    DesyncStatus status = DesyncStatus::Passed;
    std::size_t mismatchCount = 0;
    std::vector<std::uint64_t> missingEntities;
    std::vector<std::uint64_t> extraEntities;
    std::vector<OwnershipMismatch> ownershipMismatches;
    std::vector<PositionMismatch> positionMismatches;
    std::vector<ChecksumMismatch> checksumMismatches;
    std::vector<std::string> diagnostics;
};

struct StateCheckOptions
{
    double quantizationScale = 1000.0;
};

[[nodiscard]] const char* ToString(DesyncStatus status) noexcept;

[[nodiscard]] StateSnapshot CanonicalizeSnapshot(StateSnapshot snapshot);

[[nodiscard]] EntityChecksum ComputeEntityChecksum(
    const EntitySnapshot& entity,
    const StateCheckOptions& options = {});

[[nodiscard]] DesyncReport CompareSnapshots(
    StateSnapshot expected,
    StateSnapshot actual,
    const StateCheckOptions& options = {});

} // namespace SagaStateCheck
