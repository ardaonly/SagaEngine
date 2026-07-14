#include <Services/Persistence/EventSourcing/EventLog.h>
#include <SagaEngine/Core/Log/Log.h>
#include <chrono>
#include <thread>
#include <condition_variable>
#include <future>

namespace SagaEngine::Persistence {

struct EventLog::Impl {
    struct PendingEvent {
        EventRecord record;
        std::promise<uint64_t> promise;
        
        PendingEvent() = default;
        explicit PendingEvent(EventRecord&& r) : record(std::move(r)) {}
    };

    std::deque<PendingEvent> queue;
    std::mutex queueMutex;
    std::condition_variable queueCV;
    std::thread workerThread;
    std::atomic<bool> shutdown{false};
    
    std::atomic<uint64_t> totalEvents{0};
    std::atomic<uint64_t> failedCommits{0};
    std::atomic<float> avgCommitLatencyMs{0.0f};
};

EventLog::EventLog(std::shared_ptr<IDatabase> database, const EventLogConfig& config)
    : _database(std::move(database))
    , _config(config)
    , _pimpl(std::make_unique<Impl>())
{
    if (!_database || !_database->IsHealthy()) {
        LOG_ERROR("EventLog", "Invalid database instance");
        return;
    }

    _isHealthy.store(true, std::memory_order_release);
    _pimpl->workerThread = std::thread([this]() { WorkerLoop(); });
    LOG_INFO("EventLog", "Initialized (batch=%u, interval=%ums)", _config.batchSize, _config.flushIntervalMs);
}

EventLog::~EventLog() {
    _pimpl->shutdown.store(true, std::memory_order_release);
    _pimpl->queueCV.notify_all();
    if (_pimpl->workerThread.joinable()) {
        _pimpl->workerThread.join();
    }
    Flush();
    LOG_INFO("EventLog", "Shutdown complete (total=%llu, failed=%llu)", 
        _pimpl->totalEvents.load(), _pimpl->failedCommits.load());
}

uint64_t EventLog::Append(const std::string& type, const std::vector<uint8_t>& data, EntityId entityId) {
    if (!_isHealthy.load(std::memory_order_acquire)) {
        return 0;
    }

    {
        std::unique_lock lock(_pimpl->queueMutex);
        if (_pimpl->queue.size() >= _config.maxQueueSize) {
            LOG_WARN("EventLog", "Queue full, dropping event");
            return 0;
        }
    }

    uint64_t seq = _nextSequence.fetch_add(1, std::memory_order_acq_rel);
    
    EventRecord record;
    record.sequenceId = seq;
    record.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    record.type = type;
    record.data = data;
    record.entityId = entityId;

    {
        std::lock_guard lock(_pimpl->queueMutex);
        _pimpl->queue.emplace_back(std::move(record));
    }
    _pimpl->queueCV.notify_one();
    _pimpl->totalEvents.fetch_add(1, std::memory_order_relaxed);

    return seq;
}

void EventLog::Flush() {
    std::vector<EventRecord> batch;
    {
        std::lock_guard lock(_pimpl->queueMutex);
        if (_pimpl->queue.empty()) return;
        batch.reserve(_pimpl->queue.size());
        for (auto& item : _pimpl->queue) {
            batch.push_back(std::move(item.record));
        }
        _pimpl->queue.clear();
    }
    if (!batch.empty()) {
        CommitBatch(batch);
    }
}

bool EventLog::ReplayFrom(uint64_t sequenceId, EventReplayCallback callback) {
    LOG_INFO("EventLog", "Replay requested from sequence %llu", sequenceId);
    return true;
}

EventLog::Stats EventLog::GetStatistics() const {
    Stats stats;
    stats.totalEvents = _pimpl->totalEvents.load(std::memory_order_acquire);
    stats.pendingEvents = _pimpl->queue.size();
    stats.failedCommits = _pimpl->failedCommits.load(std::memory_order_acquire);
    stats.avgCommitLatencyMs = _pimpl->avgCommitLatencyMs.load(std::memory_order_acquire);
    return stats;
}

void EventLog::WorkerLoop() {
    SAGA_PROFILE_SCOPE("EventLog::WorkerLoop");
    std::vector<EventRecord> batch;
    batch.reserve(_config.batchSize);
    auto lastFlush = std::chrono::steady_clock::now();

    while (!_pimpl->shutdown.load(std::memory_order_acquire)) {
        {
            std::unique_lock lock(_pimpl->queueMutex);
            auto waitResult = _pimpl->queueCV.wait_for(lock, 
                std::chrono::milliseconds(_config.flushIntervalMs), 
                [this]() { 
                    return _pimpl->shutdown.load(std::memory_order_acquire) || 
                           !_pimpl->queue.empty(); 
                });
            
            if (_pimpl->queue.empty() && !waitResult) {
                auto now = std::chrono::steady_clock::now();
                if (std::chrono::duration_cast<std::chrono::milliseconds>(now - lastFlush).count() >= _config.flushIntervalMs) {
                } else {
                    continue; 
                }
            }

            size_t count = std::min(_pimpl->queue.size(), static_cast<size_t>(_config.batchSize));
            for (size_t i = 0; i < count; ++i) {
                batch.push_back(std::move(_pimpl->queue.front().record));
                _pimpl->queue.pop_front();
            }
        }

        if (!batch.empty()) {
            auto start = std::chrono::steady_clock::now();
            bool success = CommitBatch(batch);
            auto end = std::chrono::steady_clock::now();
            
            float latency = std::chrono::duration<float, std::milli>(end - start).count();
            _pimpl->avgCommitLatencyMs.store(latency, std::memory_order_relaxed);

            if (!success) {
                _pimpl->failedCommits.fetch_add(batch.size(), std::memory_order_relaxed);
                LOG_ERROR("EventLog", "Commit failed for %zu events", batch.size());
            } else {
                _committedSequence.store(batch.back().sequenceId, std::memory_order_release);
            }
            batch.clear();
            lastFlush = std::chrono::steady_clock::now();
        }
    }
}

bool EventLog::CommitBatch(std::vector<EventRecord>& batch) {
    SAGA_PROFILE_SCOPE("EventLog::CommitBatch");
    if (!_database->IsHealthy()) return false;

    std::vector<uint8_t> buffer;
    size_t headerSize = sizeof(uint32_t);
    buffer.reserve(headerSize + (batch.size() * 128));

    uint32_t count = static_cast<uint32_t>(batch.size());
    buffer.insert(buffer.end(), 
        reinterpret_cast<const uint8_t*>(&count), 
        reinterpret_cast<const uint8_t*>(&count) + sizeof(count));

    for (const auto& evt : batch) {
        uint32_t typeLen = static_cast<uint32_t>(evt.type.size());
        uint32_t dataLen = static_cast<uint32_t>(evt.data.size());
        
        buffer.insert(buffer.end(), 
            reinterpret_cast<const uint8_t*>(&evt.sequenceId), 
            reinterpret_cast<const uint8_t*>(&evt.sequenceId) + sizeof(evt.sequenceId));
        buffer.insert(buffer.end(), 
            reinterpret_cast<const uint8_t*>(&typeLen), 
            reinterpret_cast<const uint8_t*>(&typeLen) + sizeof(typeLen));
        buffer.insert(buffer.end(), evt.type.begin(), evt.type.end());
        buffer.insert(buffer.end(), 
            reinterpret_cast<const uint8_t*>(&dataLen), 
            reinterpret_cast<const uint8_t*>(&dataLen) + sizeof(dataLen));
        buffer.insert(buffer.end(), evt.data.begin(), evt.data.end());
    }

    std::atomic<bool> done{false};
    std::atomic<bool> success{false};
    
    _database->AppendEvent("BATCH_COMMIT", buffer, [&](bool ok, const std::string&) {
        success.store(ok, std::memory_order_release);
        done.store(true, std::memory_order_release);
    });

    while (!done.load(std::memory_order_acquire)) {
        std::this_thread::yield();
    }

    return success.load(std::memory_order_acquire);
}

} // namespace SagaEngine::Persistence