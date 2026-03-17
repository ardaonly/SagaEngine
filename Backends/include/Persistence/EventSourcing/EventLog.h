// Backends/include/Persistence/EventSourcing/EventLog.h
#pragma once
#include <Persistence/IDatabase.h>
#include <Persistence/Types.h>
#include <SagaEngine/Core/Profiling/Profiler.h>
#include <vector>
#include <deque>
#include <mutex>
#include <atomic>
#include <memory>
#include <functional>

namespace SagaEngine::Persistence {

constexpr EntityId INVALID_ENTITY_ID = UINT32_MAX;

struct EventRecord {
    uint64_t sequenceId{0};
    uint64_t timestamp{0};
    std::string type;
    std::vector<uint8_t> data;
    EntityId entityId{INVALID_ENTITY_ID};
};

struct EventLogConfig {
    uint32_t batchSize{100};
    uint32_t flushIntervalMs{500};
    uint32_t maxQueueSize{4096};
    bool synchronousValidation{false};
};

class EventLog {
public:
    explicit EventLog(std::shared_ptr<IDatabase> database, const EventLogConfig& config = {});
    ~EventLog();

    EventLog(const EventLog&) = delete;
    EventLog& operator=(const EventLog&) = delete;

    uint64_t Append(const std::string& type, const std::vector<uint8_t>& data, EntityId entityId = INVALID_ENTITY_ID);
    
    using EventReplayCallback = std::function<void(const EventRecord&)>;
    bool ReplayFrom(uint64_t sequenceId, EventReplayCallback callback);

    uint64_t GetCurrentSequence() const { return _nextSequence.load(std::memory_order_acquire); }
    void Flush();
    bool IsHealthy() const { return _isHealthy.load(std::memory_order_acquire); }

    struct Stats {
        uint64_t totalEvents{0};
        uint64_t pendingEvents{0};
        uint64_t failedCommits{0};
        float avgCommitLatencyMs{0.0f};
    };
    Stats GetStatistics() const;

private:
    struct Impl;
    std::unique_ptr<Impl> _pimpl;
    std::shared_ptr<IDatabase> _database;
    EventLogConfig _config;
    std::atomic<bool> _isHealthy{false};
    std::atomic<uint64_t> _nextSequence{1};
    std::atomic<uint64_t> _committedSequence{0};
    
    void WorkerLoop();
    bool CommitBatch(std::vector<EventRecord>& batch);
};

} // namespace SagaEngine::Persistence