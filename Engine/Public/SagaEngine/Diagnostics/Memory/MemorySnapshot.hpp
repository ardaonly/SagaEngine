/// @file MemorySnapshot.hpp
/// @brief Captures deterministic diagnostics memory records.

#pragma once

#include <SagaEngine/Diagnostics/Memory/MemoryRecord.hpp>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace SagaEngine::Diagnostics
{

/// Point-in-time memory summary for explicitly tracked diagnostics systems.
struct MemorySnapshotSummary
{
    std::size_t systemCount = 0;           ///< Number of systems with memory records.
    std::uint64_t currentBytes = 0;        ///< Total active bytes across systems.
    std::uint64_t peakBytes = 0;           ///< Sum of per-system peak bytes.
    std::uint64_t totalAllocatedBytes = 0; ///< Total explicitly allocated bytes.
    std::uint64_t totalFreedBytes = 0;     ///< Total explicitly freed bytes.
    std::uint64_t activeAllocationCount = 0; ///< Total active explicit allocations.
};

/// Immutable memory tracker snapshot with stable record ordering.
class MemorySnapshot
{
public:
    MemorySnapshot();
    /// Capture sorted records and compute aggregate counters.
    explicit MemorySnapshot(std::vector<MemoryRecord> records);

    [[nodiscard]] const std::vector<MemoryRecord>& Records() const noexcept;
    [[nodiscard]] const MemorySnapshotSummary& Summary() const noexcept;

private:
    std::vector<MemoryRecord> records_;
    MemorySnapshotSummary summary_;
};

} // namespace SagaEngine::Diagnostics
