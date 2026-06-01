/// @file ResourceSnapshot.cpp
/// @brief Implements deterministic diagnostics resource snapshots.

#include <SagaEngine/Diagnostics/Resource/ResourceSnapshot.hpp>

#include <algorithm>
#include <utility>

namespace SagaEngine::Diagnostics
{

const char* ToString(ResourceType type) noexcept
{
    switch (type)
    {
        case ResourceType::Socket: return "socket";
        case ResourceType::File: return "file";
        case ResourceType::Timer: return "timer";
        case ResourceType::Job: return "job";
        case ResourceType::AssetHandle: return "asset_handle";
        case ResourceType::DatabaseConnection: return "database_connection";
        case ResourceType::Thread: return "thread";
        case ResourceType::Other: return "other";
    }
    return "other";
}

ResourceSnapshot::ResourceSnapshot() = default;

ResourceSnapshot::ResourceSnapshot(std::vector<ResourceRecord> records)
    : records_(std::move(records))
{
    std::sort(records_.begin(),
              records_.end(),
              [](const ResourceRecord& lhs, const ResourceRecord& rhs) {
                  return lhs.resourceId < rhs.resourceId;
              });
}

const std::vector<ResourceRecord>& ResourceSnapshot::Records() const noexcept
{
    return records_;
}

ResourceLeakSummary ResourceSnapshot::BuildActiveSummary() const
{
    ResourceLeakSummary summary;
    for (const auto& record : records_)
    {
        if (record.released)
        {
            continue;
        }

        ++summary.totalActive;
        ++summary.byType[ToString(record.resourceType)];
        ++summary.byOwnerSystem[record.ownerSystem];
    }
    return summary;
}

} // namespace SagaEngine::Diagnostics
