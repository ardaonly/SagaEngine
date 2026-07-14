/// @file MemoryTracker.hpp
/// @brief Tracks explicit per-system memory counters for diagnostics.

#pragma once

#include <SagaEngine/Diagnostics/Memory/MemorySnapshot.hpp>

#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>

namespace SagaEngine::Diagnostics
{

/// Explicit diagnostics memory tracker; it does not hook allocation APIs.
class MemoryTracker
{
public:
    /// Add active bytes for a system without changing allocation count.
    bool AddBytes(std::string systemName, std::uint64_t bytes);
    /// Remove active bytes for a system without changing allocation count.
    bool RemoveBytes(std::string systemName, std::uint64_t bytes);
    /// Record one explicit allocation for a system.
    bool RecordAllocation(std::string systemName, std::uint64_t bytes);
    /// Record one explicit free for a system.
    bool RecordFree(std::string systemName, std::uint64_t bytes);

    [[nodiscard]] MemorySnapshot Snapshot() const;
    void Clear();

private:
    [[nodiscard]] MemoryRecord& FindOrCreateRecord(std::string systemName);

    mutable std::mutex mutex_;
    std::unordered_map<std::string, MemoryRecord> records_;
};

} // namespace SagaEngine::Diagnostics
