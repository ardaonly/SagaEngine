/// @file MemorySnapshot.cpp
/// @brief Implements deterministic diagnostics memory snapshots.

#include <SagaEngine/Diagnostics/Memory/MemorySnapshot.hpp>

#include <algorithm>
#include <limits>
#include <utility>

namespace SagaEngine::Diagnostics
{
namespace
{

void Accumulate(std::uint64_t& target, std::uint64_t value) noexcept
{
    const auto max = std::numeric_limits<std::uint64_t>::max();
    target = value > max - target ? max : target + value;
}

} // namespace

MemorySnapshot::MemorySnapshot() = default;

MemorySnapshot::MemorySnapshot(std::vector<MemoryRecord> records)
    : records_(std::move(records))
{
    std::sort(records_.begin(),
              records_.end(),
              [](const MemoryRecord& lhs, const MemoryRecord& rhs) {
                  return lhs.systemName < rhs.systemName;
              });

    summary_.systemCount = records_.size();
    for (const auto& record : records_)
    {
        Accumulate(summary_.currentBytes, record.currentBytes);
        Accumulate(summary_.peakBytes, record.peakBytes);
        Accumulate(summary_.totalAllocatedBytes, record.totalAllocatedBytes);
        Accumulate(summary_.totalFreedBytes, record.totalFreedBytes);
        Accumulate(summary_.activeAllocationCount,
                   record.activeAllocationCount);
    }
}

const std::vector<MemoryRecord>& MemorySnapshot::Records() const noexcept
{
    return records_;
}

const MemorySnapshotSummary& MemorySnapshot::Summary() const noexcept
{
    return summary_;
}

} // namespace SagaEngine::Diagnostics
