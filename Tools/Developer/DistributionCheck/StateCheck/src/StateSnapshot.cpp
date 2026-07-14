/// @file StateSnapshot.cpp
/// @brief Implements deterministic state snapshot comparison.

#include "SagaStateCheck/StateSnapshot.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace SagaStateCheck
{
namespace
{

constexpr std::uint64_t kFnvOffset = 14695981039346656037ull;
constexpr std::uint64_t kFnvPrime = 1099511628211ull;

[[nodiscard]] bool EntityLess(const EntitySnapshot& lhs,
                              const EntitySnapshot& rhs) noexcept
{
    if (lhs.entityId != rhs.entityId)
    {
        return lhs.entityId < rhs.entityId;
    }
    return lhs.ownerClientId < rhs.ownerClientId;
}

[[nodiscard]] std::int64_t Quantize(double value, double scale)
{
    return static_cast<std::int64_t>(std::llround(value * scale));
}

void HashBytes(std::uint64_t& hash, const void* data, std::size_t size)
{
    const auto* bytes = static_cast<const unsigned char*>(data);
    for (std::size_t i = 0; i < size; ++i)
    {
        hash ^= static_cast<std::uint64_t>(bytes[i]);
        hash *= kFnvPrime;
    }
}

template <typename T>
void HashValue(std::uint64_t& hash, const T& value)
{
    HashBytes(hash, &value, sizeof(T));
}

[[nodiscard]] bool SameQuantizedPosition(const StateVector3& lhs,
                                         const StateVector3& rhs,
                                         double scale)
{
    return Quantize(lhs.x, scale) == Quantize(rhs.x, scale) &&
           Quantize(lhs.y, scale) == Quantize(rhs.y, scale) &&
           Quantize(lhs.z, scale) == Quantize(rhs.z, scale);
}

void MarkFailed(DesyncReport& report, std::string diagnostic)
{
    report.status = DesyncStatus::Failed;
    report.diagnostics.push_back(std::move(diagnostic));
}

} // namespace

const char* ToString(DesyncStatus status) noexcept
{
    switch (status)
    {
        case DesyncStatus::Passed:
            return "Passed";
        case DesyncStatus::Failed:
            return "Failed";
    }
    return "Unknown";
}

StateSnapshot CanonicalizeSnapshot(StateSnapshot snapshot)
{
    std::sort(snapshot.entities.begin(), snapshot.entities.end(), EntityLess);
    return snapshot;
}

EntityChecksum ComputeEntityChecksum(
    const EntitySnapshot& entity,
    const StateCheckOptions& options)
{
    std::uint64_t hash = kFnvOffset;
    const std::uint32_t schemaVersion = kStateSnapshotSchemaVersion;
    const std::int64_t positionX = Quantize(entity.position.x,
                                            options.quantizationScale);
    const std::int64_t positionY = Quantize(entity.position.y,
                                            options.quantizationScale);
    const std::int64_t positionZ = Quantize(entity.position.z,
                                            options.quantizationScale);
    const std::uint8_t hasVelocity = entity.hasVelocity ? 1u : 0u;
    const std::int64_t velocityX = entity.hasVelocity
        ? Quantize(entity.velocity.x, options.quantizationScale)
        : 0;
    const std::int64_t velocityY = entity.hasVelocity
        ? Quantize(entity.velocity.y, options.quantizationScale)
        : 0;
    const std::int64_t velocityZ = entity.hasVelocity
        ? Quantize(entity.velocity.z, options.quantizationScale)
        : 0;

    HashValue(hash, schemaVersion);
    HashValue(hash, entity.entityId);
    HashValue(hash, entity.ownerClientId);
    HashValue(hash, positionX);
    HashValue(hash, positionY);
    HashValue(hash, positionZ);
    HashValue(hash, hasVelocity);
    HashValue(hash, velocityX);
    HashValue(hash, velocityY);
    HashValue(hash, velocityZ);

    return EntityChecksum{hash};
}

DesyncReport CompareSnapshots(
    StateSnapshot expected,
    StateSnapshot actual,
    const StateCheckOptions& options)
{
    expected = CanonicalizeSnapshot(std::move(expected));
    actual = CanonicalizeSnapshot(std::move(actual));

    DesyncReport report;
    std::size_t expectedIndex = 0;
    std::size_t actualIndex = 0;

    while (expectedIndex < expected.entities.size() ||
           actualIndex < actual.entities.size())
    {
        if (expectedIndex >= expected.entities.size())
        {
            const auto& extra = actual.entities[actualIndex++];
            report.extraEntities.push_back(extra.entityId);
            MarkFailed(report, "extra entity " + std::to_string(extra.entityId));
            continue;
        }

        if (actualIndex >= actual.entities.size())
        {
            const auto& missing = expected.entities[expectedIndex++];
            report.missingEntities.push_back(missing.entityId);
            MarkFailed(report,
                       "missing entity " + std::to_string(missing.entityId));
            continue;
        }

        const auto& expectedEntity = expected.entities[expectedIndex];
        const auto& actualEntity = actual.entities[actualIndex];

        if (expectedEntity.entityId < actualEntity.entityId)
        {
            report.missingEntities.push_back(expectedEntity.entityId);
            MarkFailed(report,
                       "missing entity " +
                           std::to_string(expectedEntity.entityId));
            ++expectedIndex;
            continue;
        }

        if (actualEntity.entityId < expectedEntity.entityId)
        {
            report.extraEntities.push_back(actualEntity.entityId);
            MarkFailed(report,
                       "extra entity " +
                           std::to_string(actualEntity.entityId));
            ++actualIndex;
            continue;
        }

        if (expectedEntity.ownerClientId != actualEntity.ownerClientId)
        {
            report.ownershipMismatches.push_back(OwnershipMismatch{
                expectedEntity.entityId,
                expectedEntity.ownerClientId,
                actualEntity.ownerClientId,
            });
            MarkFailed(report,
                       "ownership mismatch for entity " +
                           std::to_string(expectedEntity.entityId));
        }

        if (!SameQuantizedPosition(expectedEntity.position,
                                   actualEntity.position,
                                   options.quantizationScale))
        {
            report.positionMismatches.push_back(PositionMismatch{
                expectedEntity.entityId,
                expectedEntity.position,
                actualEntity.position,
            });
            MarkFailed(report,
                       "position mismatch for entity " +
                           std::to_string(expectedEntity.entityId));
        }

        const EntityChecksum expectedChecksum =
            ComputeEntityChecksum(expectedEntity, options);
        const EntityChecksum actualChecksum =
            ComputeEntityChecksum(actualEntity, options);
        if (expectedChecksum.value != actualChecksum.value)
        {
            report.checksumMismatches.push_back(ChecksumMismatch{
                expectedEntity.entityId,
                expectedChecksum,
                actualChecksum,
            });
            MarkFailed(report,
                       "checksum mismatch for entity " +
                           std::to_string(expectedEntity.entityId));
        }

        ++expectedIndex;
        ++actualIndex;
    }

    report.mismatchCount = report.missingEntities.size() +
                           report.extraEntities.size() +
                           report.ownershipMismatches.size() +
                           report.positionMismatches.size() +
                           report.checksumMismatches.size();
    if (report.mismatchCount == 0)
    {
        report.status = DesyncStatus::Passed;
    }

    return report;
}

} // namespace SagaStateCheck
