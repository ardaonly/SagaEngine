/// @file MemoryTracker.cpp
/// @brief Implements explicit diagnostics memory tracking.

#include <SagaEngine/Diagnostics/Memory/MemoryScope.hpp>
#include <SagaEngine/Diagnostics/Memory/MemoryTracker.hpp>

#include <algorithm>
#include <limits>
#include <utility>

namespace SagaEngine::Diagnostics
{
namespace
{

[[nodiscard]] bool CanAdd(std::uint64_t current,
                          std::uint64_t delta) noexcept
{
    return delta <= std::numeric_limits<std::uint64_t>::max() - current;
}

} // namespace

bool MemoryTracker::AddBytes(std::string systemName, std::uint64_t bytes)
{
    std::lock_guard lock(mutex_);
    auto& record = FindOrCreateRecord(std::move(systemName));
    if (!CanAdd(record.currentBytes, bytes) ||
        !CanAdd(record.totalAllocatedBytes, bytes))
    {
        return false;
    }

    record.currentBytes += bytes;
    record.totalAllocatedBytes += bytes;
    record.peakBytes = std::max(record.peakBytes, record.currentBytes);
    return true;
}

bool MemoryTracker::RemoveBytes(std::string systemName, std::uint64_t bytes)
{
    std::lock_guard lock(mutex_);
    auto it = records_.find(systemName);
    if (it == records_.end() || it->second.currentBytes < bytes ||
        !CanAdd(it->second.totalFreedBytes, bytes))
    {
        return false;
    }

    it->second.currentBytes -= bytes;
    it->second.totalFreedBytes += bytes;
    return true;
}

bool MemoryTracker::RecordAllocation(std::string systemName,
                                     std::uint64_t bytes)
{
    std::lock_guard lock(mutex_);
    auto& record = FindOrCreateRecord(std::move(systemName));
    if (!CanAdd(record.currentBytes, bytes) ||
        !CanAdd(record.totalAllocatedBytes, bytes) ||
        !CanAdd(record.activeAllocationCount, 1))
    {
        return false;
    }

    record.currentBytes += bytes;
    record.totalAllocatedBytes += bytes;
    ++record.activeAllocationCount;
    record.peakBytes = std::max(record.peakBytes, record.currentBytes);
    return true;
}

bool MemoryTracker::RecordFree(std::string systemName, std::uint64_t bytes)
{
    std::lock_guard lock(mutex_);
    auto it = records_.find(systemName);
    if (it == records_.end() || it->second.currentBytes < bytes ||
        it->second.activeAllocationCount == 0 ||
        !CanAdd(it->second.totalFreedBytes, bytes))
    {
        return false;
    }

    it->second.currentBytes -= bytes;
    it->second.totalFreedBytes += bytes;
    --it->second.activeAllocationCount;
    return true;
}

MemorySnapshot MemoryTracker::Snapshot() const
{
    std::lock_guard lock(mutex_);
    std::vector<MemoryRecord> records;
    records.reserve(records_.size());
    for (const auto& [systemName, record] : records_)
    {
        (void)systemName;
        records.push_back(record);
    }
    return MemorySnapshot(std::move(records));
}

void MemoryTracker::Clear()
{
    std::lock_guard lock(mutex_);
    records_.clear();
}

MemoryRecord& MemoryTracker::FindOrCreateRecord(std::string systemName)
{
    auto [it, inserted] = records_.try_emplace(systemName);
    if (inserted)
    {
        it->second.systemName = std::move(systemName);
    }
    return it->second;
}

MemoryScope::MemoryScope(MemoryTracker& tracker,
                         std::string systemName,
                         std::uint64_t bytes)
    : tracker_(&tracker)
    , systemName_(std::move(systemName))
    , bytes_(bytes)
    , active_(tracker_->RecordAllocation(systemName_, bytes_))
{
}

MemoryScope::~MemoryScope()
{
    (void)Release();
}

MemoryScope::MemoryScope(MemoryScope&& other) noexcept
    : tracker_(other.tracker_)
    , systemName_(std::move(other.systemName_))
    , bytes_(other.bytes_)
    , active_(other.active_)
{
    other.tracker_ = nullptr;
    other.bytes_ = 0;
    other.active_ = false;
}

MemoryScope& MemoryScope::operator=(MemoryScope&& other) noexcept
{
    if (this == &other)
    {
        return *this;
    }

    (void)Release();
    tracker_ = other.tracker_;
    systemName_ = std::move(other.systemName_);
    bytes_ = other.bytes_;
    active_ = other.active_;
    other.tracker_ = nullptr;
    other.bytes_ = 0;
    other.active_ = false;
    return *this;
}

bool MemoryScope::Release() noexcept
{
    if (!active_ || tracker_ == nullptr)
    {
        return false;
    }

    const bool released = tracker_->RecordFree(systemName_, bytes_);
    active_ = false;
    return released;
}

} // namespace SagaEngine::Diagnostics
