/// @file MemoryRecord.hpp
/// @brief Defines explicit per-system memory counters for diagnostics reports.

#pragma once

#include <cstdint>
#include <string>

namespace SagaEngine::Diagnostics
{

/// Explicit memory counters reported by one logical system.
struct MemoryRecord
{
    std::string systemName;                    ///< Logical owner of the tracked bytes.
    std::uint64_t currentBytes = 0;            ///< Bytes currently recorded as active.
    std::uint64_t peakBytes = 0;               ///< Highest active byte count observed.
    std::uint64_t totalAllocatedBytes = 0;     ///< Cumulative bytes explicitly added.
    std::uint64_t totalFreedBytes = 0;         ///< Cumulative bytes explicitly removed.
    std::uint64_t activeAllocationCount = 0;   ///< Count of explicit active allocations.
};

} // namespace SagaEngine::Diagnostics
