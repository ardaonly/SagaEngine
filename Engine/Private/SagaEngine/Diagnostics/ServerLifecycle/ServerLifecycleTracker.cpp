/// @file ServerLifecycleTracker.cpp
/// @brief Implements bounded server lifecycle diagnostics tracking.

#include <SagaEngine/Diagnostics/ServerLifecycle/ServerLifecycleTracker.hpp>

#include <algorithm>
#include <utility>

namespace SagaEngine::Diagnostics
{
namespace
{

void SortPayload(
    std::vector<std::pair<std::string, std::string>>& payload)
{
    std::sort(payload.begin(),
              payload.end(),
              [](const auto& lhs, const auto& rhs) {
                  if (lhs.first != rhs.first)
                  {
                      return lhs.first < rhs.first;
                  }
                  return lhs.second < rhs.second;
              });
}

} // namespace

ServerLifecycleTracker::ServerLifecycleTracker(
    std::size_t maxEvents,
    std::size_t maxRecords)
    : maxEvents_(maxEvents)
    , maxRecords_(maxRecords)
{
}

void ServerLifecycleTracker::ConfigureLimits(
    std::size_t maxEvents,
    std::size_t maxRecords)
{
    std::lock_guard<std::mutex> lock(mutex_);
    maxEvents_ = maxEvents;
    maxRecords_ = maxRecords;
}

bool ServerLifecycleTracker::RecordEvent(
    std::string eventName,
    std::string category,
    std::string severity,
    std::string message,
    std::uint32_t zoneId,
    std::uint64_t tick,
    std::vector<std::pair<std::string, std::string>> payload)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (events_.size() >= maxEvents_)
    {
        ++droppedEventCount_;
        return false;
    }

    SortPayload(payload);

    ServerLifecycleEvent event;
    event.sequence = nextEventSequence_++;
    event.eventName = std::move(eventName);
    event.category = std::move(category);
    event.severity = std::move(severity);
    event.message = std::move(message);
    event.zoneId = zoneId;
    event.tick = tick;
    event.payload = std::move(payload);
    events_.push_back(std::move(event));
    return true;
}

std::uint64_t ServerLifecycleTracker::CreateRecord(
    std::string recordKind,
    std::uint32_t zoneId,
    std::uint64_t externalId,
    std::uint64_t ownerRecordId,
    std::uint64_t createdTick,
    std::string status,
    std::string label)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (records_.size() >= maxRecords_)
    {
        ++droppedRecordCount_;
        return 0;
    }

    const std::uint64_t recordId = nextRecordId_++;
    ServerLifecycleRecord record;
    record.recordId = recordId;
    record.recordKind = std::move(recordKind);
    record.zoneId = zoneId;
    record.externalId = externalId;
    record.ownerRecordId = ownerRecordId;
    record.createdTick = createdTick;
    record.active = true;
    record.status = std::move(status);
    record.label = std::move(label);
    records_.emplace(recordId, std::move(record));
    return recordId;
}

bool ServerLifecycleTracker::DestroyRecord(
    std::uint64_t recordId,
    std::uint64_t destroyedTick,
    std::string status)
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = records_.find(recordId);
    if (it == records_.end() || !it->second.active)
    {
        return false;
    }

    it->second.active = false;
    it->second.destroyedTick = destroyedTick;
    if (!status.empty())
    {
        it->second.status = std::move(status);
    }
    return true;
}

bool ServerLifecycleTracker::DestroyActiveRecordByExternalId(
    std::string recordKind,
    std::uint32_t zoneId,
    std::uint64_t externalId,
    std::uint64_t destroyedTick,
    std::string status)
{
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& [_, record] : records_)
    {
        if (record.active &&
            record.recordKind == recordKind &&
            record.zoneId == zoneId &&
            record.externalId == externalId)
        {
            record.active = false;
            record.destroyedTick = destroyedTick;
            if (!status.empty())
            {
                record.status = std::move(status);
            }
            return true;
        }
    }
    return false;
}

std::uint64_t ServerLifecycleTracker::FindActiveRecordId(
    std::string recordKind,
    std::uint32_t zoneId,
    std::uint64_t externalId) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& [recordId, record] : records_)
    {
        if (record.active &&
            record.recordKind == recordKind &&
            record.zoneId == zoneId &&
            record.externalId == externalId)
        {
            return recordId;
        }
    }
    return 0;
}

std::size_t ServerLifecycleTracker::EmitLifetimeLeakEventsForActiveRecords(
    std::uint32_t zoneId,
    std::uint64_t tick,
    std::string message)
{
    std::vector<ServerLifecycleRecord> activeRecords;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        activeRecords.reserve(records_.size());
        for (const auto& [_, record] : records_)
        {
            if (record.active && record.zoneId == zoneId)
            {
                activeRecords.push_back(record);
            }
        }
        std::sort(activeRecords.begin(),
                  activeRecords.end(),
                  [](const auto& lhs, const auto& rhs) {
                      return lhs.recordId < rhs.recordId;
                  });
    }

    std::size_t emitted = 0;
    for (const auto& record : activeRecords)
    {
        if (RecordEvent(
                "LifetimeLeakDetected",
                "lifetime",
                "warning",
                message,
                zoneId,
                tick,
                {{"external_id", std::to_string(record.externalId)},
                 {"label", record.label},
                 {"record_id", std::to_string(record.recordId)},
                 {"record_kind", record.recordKind},
                 {"status", record.status}}))
        {
            ++emitted;
        }
    }
    return emitted;
}

ServerLifecycleSnapshot ServerLifecycleTracker::Snapshot() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return SnapshotLocked();
}

void ServerLifecycleTracker::Clear()
{
    std::lock_guard<std::mutex> lock(mutex_);
    events_.clear();
    records_.clear();
    nextEventSequence_ = 1;
    nextRecordId_ = 1;
    droppedEventCount_ = 0;
    droppedRecordCount_ = 0;
}

ServerLifecycleSnapshot ServerLifecycleTracker::SnapshotLocked() const
{
    ServerLifecycleSnapshot snapshot;
    snapshot.events = events_;
    snapshot.records.reserve(records_.size());
    for (const auto& [_, record] : records_)
    {
        snapshot.records.push_back(record);
        if (record.active)
        {
            snapshot.leaks.push_back(record);
        }
    }

    std::sort(snapshot.events.begin(),
              snapshot.events.end(),
              [](const auto& lhs, const auto& rhs) {
                  return lhs.sequence < rhs.sequence;
              });
    std::sort(snapshot.records.begin(),
              snapshot.records.end(),
              [](const auto& lhs, const auto& rhs) {
                  return lhs.recordId < rhs.recordId;
              });
    std::sort(snapshot.leaks.begin(),
              snapshot.leaks.end(),
              [](const auto& lhs, const auto& rhs) {
                  return lhs.recordId < rhs.recordId;
              });

    snapshot.summary.eventCount = snapshot.events.size();
    snapshot.summary.recordCount = snapshot.records.size();
    snapshot.summary.activeRecordCount = snapshot.leaks.size();
    snapshot.summary.leakCount = snapshot.leaks.size();
    snapshot.summary.droppedEventCount = droppedEventCount_;
    snapshot.summary.droppedRecordCount = droppedRecordCount_;
    return snapshot;
}

} // namespace SagaEngine::Diagnostics
