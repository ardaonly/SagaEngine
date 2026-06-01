/// @file LifetimeTracker.cpp
/// @brief Implements logical lifetime and ownership tracking.

#include <SagaEngine/Diagnostics/Lifetime/LifetimeTracker.hpp>

#include <algorithm>
#include <utility>

namespace SagaEngine::Diagnostics
{

LifetimeHandle LifetimeTracker::Register(std::string typeName,
                                          std::string ownerSystem,
                                          std::uint64_t externalId,
                                          std::uint64_t createdTick)
{
    std::lock_guard<std::mutex> lock(mutex_);

    const auto objectId = nextObjectId_++;
    LifetimeRecord record;
    record.objectId = objectId;
    record.externalId = externalId;
    record.typeName = std::move(typeName);
    record.ownerSystem = std::move(ownerSystem);
    record.createdTick = createdTick;

    records_.emplace(objectId, std::move(record));
    return LifetimeHandle{objectId};
}

bool LifetimeTracker::MarkDestroyed(LifetimeHandle handle, std::uint64_t destroyedTick)
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = records_.find(handle.objectId);
    if (it == records_.end())
    {
        return false;
    }
    if (it->second.destroyed)
    {
        return false;
    }

    it->second.destroyed = true;
    it->second.destroyedTick = destroyedTick;
    return true;
}

bool LifetimeTracker::SetOwner(LifetimeHandle child, LifetimeHandle owner)
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto childIt = records_.find(child.objectId);
    if (childIt == records_.end() || records_.find(owner.objectId) == records_.end())
    {
        return false;
    }

    childIt->second.ownerObjectId = owner.objectId;
    return true;
}

std::vector<LifetimeRecord> LifetimeTracker::SnapshotAll() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<LifetimeRecord> result;
    result.reserve(records_.size());
    for (const auto& [_, record] : records_)
    {
        result.push_back(record);
    }
    std::sort(result.begin(),
              result.end(),
              [](const LifetimeRecord& lhs, const LifetimeRecord& rhs) {
                  return lhs.objectId < rhs.objectId;
              });
    return result;
}

std::vector<LifetimeRecord> LifetimeTracker::SnapshotActive() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<LifetimeRecord> result;
    for (const auto& [_, record] : records_)
    {
        if (!record.destroyed)
        {
            result.push_back(record);
        }
    }
    std::sort(result.begin(),
              result.end(),
              [](const LifetimeRecord& lhs, const LifetimeRecord& rhs) {
                  return lhs.objectId < rhs.objectId;
              });
    return result;
}

std::vector<LifetimeRecord> LifetimeTracker::FindUndestroyedByOwner(
    std::uint64_t ownerObjectId) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<LifetimeRecord> result;
    for (const auto& [_, record] : records_)
    {
        if (!record.destroyed && record.ownerObjectId == ownerObjectId)
        {
            result.push_back(record);
        }
    }
    std::sort(result.begin(),
              result.end(),
              [](const LifetimeRecord& lhs, const LifetimeRecord& rhs) {
                  return lhs.objectId < rhs.objectId;
              });
    return result;
}

void LifetimeTracker::Clear()
{
    std::lock_guard<std::mutex> lock(mutex_);
    records_.clear();
    nextObjectId_ = 1;
}

} // namespace SagaEngine::Diagnostics
