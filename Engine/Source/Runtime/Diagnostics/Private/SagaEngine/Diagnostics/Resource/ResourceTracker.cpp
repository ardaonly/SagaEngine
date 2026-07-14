/// @file ResourceTracker.cpp
/// @brief Implements explicit diagnostics resource tracking.

#include <SagaEngine/Diagnostics/Resource/ResourceTracker.hpp>

#include <algorithm>
#include <limits>
#include <utility>

namespace SagaEngine::Diagnostics
{

std::uint64_t ResourceTracker::RegisterResource(ResourceType type,
                                                std::string ownerSystem,
                                                std::string debugName,
                                                std::uint64_t tick)
{
    std::lock_guard lock(mutex_);
    if (nextResourceId_ == std::numeric_limits<std::uint64_t>::max())
    {
        return 0;
    }

    ResourceRecord record;
    record.resourceId = nextResourceId_++;
    record.resourceType = type;
    record.ownerSystem = std::move(ownerSystem);
    record.debugName = std::move(debugName);
    record.createdTick = tick;
    records_.emplace(record.resourceId, std::move(record));
    return nextResourceId_ - 1;
}

bool ResourceTracker::ReleaseResource(std::uint64_t resourceId,
                                      std::uint64_t tick)
{
    std::lock_guard lock(mutex_);
    auto it = records_.find(resourceId);
    if (it == records_.end() || it->second.released)
    {
        return false;
    }

    it->second.released = true;
    it->second.releasedTick = tick;
    return true;
}

ResourceSnapshot ResourceTracker::SnapshotActive() const
{
    std::lock_guard lock(mutex_);
    std::vector<ResourceRecord> records;
    records.reserve(records_.size());
    for (const auto& [resourceId, record] : records_)
    {
        (void)resourceId;
        if (!record.released)
        {
            records.push_back(record);
        }
    }
    return ResourceSnapshot(std::move(records));
}

ResourceSnapshot ResourceTracker::SnapshotAll() const
{
    std::lock_guard lock(mutex_);
    std::vector<ResourceRecord> records;
    records.reserve(records_.size());
    for (const auto& [resourceId, record] : records_)
    {
        (void)resourceId;
        records.push_back(record);
    }
    return ResourceSnapshot(std::move(records));
}

ResourceLeakSummary ResourceTracker::BuildResourceLeakSummary() const
{
    return SnapshotActive().BuildActiveSummary();
}

void ResourceTracker::Clear()
{
    std::lock_guard lock(mutex_);
    records_.clear();
    nextResourceId_ = 1;
}

} // namespace SagaEngine::Diagnostics
