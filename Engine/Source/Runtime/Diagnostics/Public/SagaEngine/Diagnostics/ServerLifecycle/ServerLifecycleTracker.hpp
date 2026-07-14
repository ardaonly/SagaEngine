/// @file ServerLifecycleTracker.hpp
/// @brief Bounded server lifecycle diagnostics contracts and tracker.

#pragma once

#include <cstdint>
#include <cstddef>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace SagaEngine::Diagnostics
{

struct ServerLifecycleEvent
{
    std::uint64_t sequence = 0;
    std::string eventName;
    std::string category;
    std::string severity;
    std::string message;
    std::uint32_t zoneId = 0;
    std::uint64_t tick = 0;
    std::vector<std::pair<std::string, std::string>> payload;
};

struct ServerLifecycleRecord
{
    std::uint64_t recordId = 0;
    std::string recordKind;
    std::uint32_t zoneId = 0;
    std::uint64_t externalId = 0;
    std::uint64_t ownerRecordId = 0;
    std::uint64_t createdTick = 0;
    std::uint64_t destroyedTick = 0;
    bool active = false;
    std::string status;
    std::string label;
};

struct ServerLifecycleSummary
{
    std::size_t eventCount = 0;
    std::size_t recordCount = 0;
    std::size_t activeRecordCount = 0;
    std::size_t leakCount = 0;
    std::size_t droppedEventCount = 0;
    std::size_t droppedRecordCount = 0;
};

struct ServerLifecycleSnapshot
{
    std::vector<ServerLifecycleEvent> events;
    std::vector<ServerLifecycleRecord> records;
    std::vector<ServerLifecycleRecord> leaks;
    ServerLifecycleSummary summary;
};

class ServerLifecycleTracker
{
public:
    ServerLifecycleTracker() = default;
    ServerLifecycleTracker(std::size_t maxEvents, std::size_t maxRecords);

    void ConfigureLimits(std::size_t maxEvents, std::size_t maxRecords);

    [[nodiscard]] bool RecordEvent(
        std::string eventName,
        std::string category,
        std::string severity,
        std::string message,
        std::uint32_t zoneId = 0,
        std::uint64_t tick = 0,
        std::vector<std::pair<std::string, std::string>> payload = {});

    [[nodiscard]] std::uint64_t CreateRecord(
        std::string recordKind,
        std::uint32_t zoneId,
        std::uint64_t externalId = 0,
        std::uint64_t ownerRecordId = 0,
        std::uint64_t createdTick = 0,
        std::string status = {},
        std::string label = {});

    [[nodiscard]] bool DestroyRecord(
        std::uint64_t recordId,
        std::uint64_t destroyedTick = 0,
        std::string status = {});

    [[nodiscard]] bool DestroyActiveRecordByExternalId(
        std::string recordKind,
        std::uint32_t zoneId,
        std::uint64_t externalId,
        std::uint64_t destroyedTick = 0,
        std::string status = {});

    [[nodiscard]] std::uint64_t FindActiveRecordId(
        std::string recordKind,
        std::uint32_t zoneId,
        std::uint64_t externalId) const;

    [[nodiscard]] std::size_t EmitLifetimeLeakEventsForActiveRecords(
        std::uint32_t zoneId,
        std::uint64_t tick = 0,
        std::string message = "Active lifecycle record remained at transition");

    [[nodiscard]] ServerLifecycleSnapshot Snapshot() const;

    void Clear();

private:
    [[nodiscard]] ServerLifecycleSnapshot SnapshotLocked() const;

    mutable std::mutex mutex_;
    std::vector<ServerLifecycleEvent> events_;
    std::unordered_map<std::uint64_t, ServerLifecycleRecord> records_;
    std::uint64_t nextEventSequence_ = 1;
    std::uint64_t nextRecordId_ = 1;
    std::size_t maxEvents_ = 512;
    std::size_t maxRecords_ = 512;
    std::size_t droppedEventCount_ = 0;
    std::size_t droppedRecordCount_ = 0;
};

} // namespace SagaEngine::Diagnostics
