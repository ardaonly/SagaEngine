#include <Persistence/Database/RedisImpl.h>
#include <SagaEngine/Core/Log/Log.h>
#include <SagaEngine/Core/Profiling/Profiler.h>
#include <sw/redis++/redis++.h>
#include <queue>
#include <thread>
#include <chrono>
#include <mutex>
#include <condition_variable>

namespace SagaEngine::Persistence {

struct RedisDatabase::Impl {
    struct WriteRequest {
        std::string key;
        std::vector<uint8_t> value;
        DatabaseCallback callback;
        uint64_t enqueueTime;
    };

    std::unique_ptr<sw::redis::Redis> redis;
    
    std::queue<WriteRequest> writeQueue;
    std::mutex queueMutex;
    std::condition_variable queueCV;
    std::thread workerThread;
    std::atomic<bool> shutdown{false};
    
    std::atomic<uint32_t> pendingWrites{0};
    std::atomic<float> avgWriteLatencyMs{0.0f};
};

RedisDatabase::RedisDatabase(const RedisConfig& config)
: _config(config)
, _pimpl(std::make_unique<Impl>())
{
}

RedisDatabase::~RedisDatabase() {
    Shutdown();
}

bool RedisDatabase::Initialize() {
    SAGA_PROFILE_SCOPE("Redis::Initialize");
    
    try {
        sw::redis::ConnectionOptions opts;
        opts.host = _config.host;
        opts.port = _config.port;
        if (!_config.password.empty()) {
            opts.password = _config.password;
        }
        
        sw::redis::ConnectionPoolOptions poolOpts;
        poolOpts.size = std::max(1u, _config.maxConnectionPool);
        
        _pimpl->redis = std::make_unique<sw::redis::Redis>(opts, poolOpts);
        _pimpl->redis->ping();
        
        _pimpl->workerThread = std::thread([this]() { ProcessWriteQueue(); });
        _isHealthy.store(true, std::memory_order_release);
        
        LOG_INFO("Redis", "Initialized (host=%s, port=%u)", _config.host.c_str(), _config.port);
        return true;
    }
    catch (const std::exception& e) {
        LOG_ERROR("Redis", "Initialization failed: %s", e.what());
        return false;
    }
}

void RedisDatabase::Shutdown() {
    SAGA_PROFILE_SCOPE("Redis::Shutdown");
    
    _pimpl->shutdown.store(true, std::memory_order_release);
    _pimpl->queueCV.notify_all();
    
    if (_pimpl->workerThread.joinable()) {
        _pimpl->workerThread.join();
    }
    
    _pimpl->redis.reset();
    _isHealthy.store(false, std::memory_order_release);
    
    LOG_INFO("Redis", "Shutdown (writes=%llu, errors=%llu)", 
             _totalWrites.load(), _totalErrors.load());
}

void RedisDatabase::WriteEntity(const EntitySnapshot& snapshot, DatabaseCallback cb) {
    SAGA_PROFILE_SCOPE("Redis::WriteEntity");
    std::string key = "entity:" + std::to_string(snapshot.entityId);
    SetCache(key, snapshot.data, cb);
}

void RedisDatabase::ReadEntity(EntityId entityId, DatabaseCallback cb) {
    SAGA_PROFILE_SCOPE("Redis::ReadEntity");
    std::string key = "entity:" + std::to_string(entityId);
    GetCache(key, cb);
}

void RedisDatabase::DeleteEntity(EntityId entityId, DatabaseCallback cb) {
    std::string key = "entity:" + std::to_string(entityId);
    DeleteCache(key, cb);
}

void RedisDatabase::AppendEvent(const std::string& type, const std::vector<uint8_t>& data, DatabaseCallback cb) {
    std::string key = "event:" + type + ":" + std::to_string(std::time(nullptr));
    SetCache(key, data, cb);
}

void RedisDatabase::SetCache(const std::string& key, const std::vector<uint8_t>& value, DatabaseCallback cb) {
    if (!_isHealthy.load(std::memory_order_acquire)) {
        if (cb) cb(false, "Redis not healthy");
        _totalErrors.fetch_add(1, std::memory_order_relaxed);
        return;
    }
    
    {
        std::lock_guard<std::mutex> lock(_pimpl->queueMutex);
        Impl::WriteRequest request;
        request.key = key;
        request.value = value;
        request.callback = std::move(cb);
        request.enqueueTime = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
        _pimpl->writeQueue.push(std::move(request));
    }
    
    _pimpl->pendingWrites.fetch_add(1, std::memory_order_relaxed);
    _pimpl->queueCV.notify_one();
}

void RedisDatabase::GetCache(const std::string& key, DatabaseCallback cb) {
    SAGA_PROFILE_SCOPE("Redis::GetCache");
    
    auto startTime = std::chrono::steady_clock::now();
    
    try {
        auto val = _pimpl->redis->get(key);
        
        auto endTime = std::chrono::steady_clock::now();
        float latency = std::chrono::duration<float, std::milli>(endTime - startTime).count();
        _pimpl->avgWriteLatencyMs.store(latency, std::memory_order_relaxed);
        _totalReads.fetch_add(1, std::memory_order_relaxed);
        
        if (cb) cb(val.has_value(), val.has_value() ? "" : "Key not found");
    }
    catch (const std::exception& e) {
        LOG_ERROR("Redis", "GetCache failed: %s", e.what());
        if (cb) cb(false, e.what());
        _totalErrors.fetch_add(1, std::memory_order_relaxed);
    }
}

void RedisDatabase::DeleteCache(const std::string& key, DatabaseCallback cb) {
    try {
        _pimpl->redis->del(key);
        if (cb) cb(true, "");
        _totalWrites.fetch_add(1, std::memory_order_relaxed);
    }
    catch (const std::exception& e) {
        LOG_ERROR("Redis", "DeleteCache failed: %s", e.what());
        if (cb) cb(false, e.what());
        _totalErrors.fetch_add(1, std::memory_order_relaxed);
    }
}

void RedisDatabase::ProcessWriteQueue() {
    SAGA_PROFILE_SCOPE("Redis::WorkerThread");
    
    while (!_pimpl->shutdown.load(std::memory_order_acquire)) {
        Impl::WriteRequest request;
        
        {
            std::unique_lock<std::mutex> lock(_pimpl->queueMutex);
            _pimpl->queueCV.wait_for(lock, std::chrono::milliseconds(100), [this]() {
                return !_pimpl->writeQueue.empty() || _pimpl->shutdown.load(std::memory_order_acquire);
            });
            
            if (_pimpl->writeQueue.empty()) continue;
            request = std::move(_pimpl->writeQueue.front());
            _pimpl->writeQueue.pop();
        }
        
        auto startTime = std::chrono::steady_clock::now();
        bool success = false;
        std::string error;
        
        try {
            std::string valueStr(request.value.begin(), request.value.end());
            _pimpl->redis->set(request.key, 
                              sw::redis::StringView(valueStr.data(), valueStr.size()),
                              std::chrono::seconds(_config.ttlSeconds));
            success = true;
        }
        catch (const std::exception& e) {
            error = e.what();
            LOG_ERROR("Redis", "Write failed: %s", e.what());
        }
        
        auto endTime = std::chrono::steady_clock::now();
        float latency = std::chrono::duration<float, std::milli>(endTime - startTime).count();
        _pimpl->avgWriteLatencyMs.store(latency, std::memory_order_relaxed);
        
        if (success) _totalWrites.fetch_add(1, std::memory_order_relaxed);
        else _totalErrors.fetch_add(1, std::memory_order_relaxed);
        
        _pimpl->pendingWrites.fetch_sub(1, std::memory_order_relaxed);
        
        if (request.callback) request.callback(success, error);
    }
}

DatabaseStats RedisDatabase::GetStatistics() const {
    DatabaseStats stats;
    stats.totalWrites = _totalWrites.load(std::memory_order_acquire);
    stats.totalReads = _totalReads.load(std::memory_order_acquire);
    stats.totalErrors = _totalErrors.load(std::memory_order_acquire);
    stats.averageWriteLatencyMs = _pimpl->avgWriteLatencyMs.load(std::memory_order_acquire);
    stats.pendingWrites = _pimpl->pendingWrites.load(std::memory_order_acquire);
    return stats;
}

} // namespace SagaEngine::Persistence