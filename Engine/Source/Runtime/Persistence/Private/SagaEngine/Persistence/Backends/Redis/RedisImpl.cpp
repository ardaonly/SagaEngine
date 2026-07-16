#include "SagaEngine/Persistence/Backends/Redis/RedisImpl.h"
#include <SagaEngine/Core/Log/Log.h>
#include <SagaEngine/Core/Profiling/Profiler.h>
#include <sw/redis++/redis++.h>
#include <queue>
#include <thread>
#include <chrono>
#include <mutex>
#include <condition_variable>
#include <cstring>
#include <optional>
#include <type_traits>

namespace {

using SagaEngine::Persistence::ComponentTypeId;
using SagaEngine::Persistence::DatabaseError;
using SagaEngine::Persistence::DatabaseStatus;
using SagaEngine::Persistence::EntitySnapshot;

constexpr uint32_t kSnapshotMagic = 0x53414741u;

[[nodiscard]] DatabaseStatus Failure(DatabaseError error, std::string message)
{
    return DatabaseStatus::Failure(error, std::move(message));
}

template<typename T>
void AppendScalar(std::vector<uint8_t>& output, T value)
{
    static_assert(std::is_trivially_copyable_v<T>);
    const auto* begin = reinterpret_cast<const uint8_t*>(&value);
    output.insert(output.end(), begin, begin + sizeof(T));
}

template<typename T>
[[nodiscard]] bool ReadScalar(const std::vector<uint8_t>& input,
                              std::size_t& offset,
                              T& value)
{
    static_assert(std::is_trivially_copyable_v<T>);
    if (offset > input.size() || input.size() - offset < sizeof(T)) return false;
    std::memcpy(&value, input.data() + offset, sizeof(T));
    offset += sizeof(T);
    return true;
}

[[nodiscard]] std::vector<uint8_t> EncodeSnapshot(const EntitySnapshot& snapshot)
{
    std::vector<uint8_t> output;
    output.reserve(32 + snapshot.data.size() +
                   snapshot.componentTypes.size() * sizeof(ComponentTypeId));
    AppendScalar(output, kSnapshotMagic);
    AppendScalar(output, snapshot.entityId);
    AppendScalar(output, snapshot.version);
    AppendScalar(output, snapshot.timestamp);
    AppendScalar(output, static_cast<uint64_t>(snapshot.data.size()));
    AppendScalar(output, static_cast<uint64_t>(snapshot.componentTypes.size()));
    output.insert(output.end(), snapshot.data.begin(), snapshot.data.end());
    for (const auto type : snapshot.componentTypes) AppendScalar(output, type);
    return output;
}

[[nodiscard]] std::optional<EntitySnapshot> DecodeSnapshot(
    const std::vector<uint8_t>& input)
{
    std::size_t offset = 0;
    uint32_t magic = 0;
    uint64_t dataSize = 0;
    uint64_t typeCount = 0;
    EntitySnapshot snapshot;
    if (!ReadScalar(input, offset, magic) || magic != kSnapshotMagic ||
        !ReadScalar(input, offset, snapshot.entityId) ||
        !ReadScalar(input, offset, snapshot.version) ||
        !ReadScalar(input, offset, snapshot.timestamp) ||
        !ReadScalar(input, offset, dataSize) ||
        !ReadScalar(input, offset, typeCount)) {
        return std::nullopt;
    }

    if (dataSize > input.size() - offset) return std::nullopt;
    snapshot.data.assign(input.begin() + static_cast<std::ptrdiff_t>(offset),
                         input.begin() + static_cast<std::ptrdiff_t>(offset + dataSize));
    offset += static_cast<std::size_t>(dataSize);

    if (typeCount > (input.size() - offset) / sizeof(ComponentTypeId)) return std::nullopt;
    snapshot.componentTypes.reserve(static_cast<std::size_t>(typeCount));
    for (uint64_t index = 0; index < typeCount; ++index) {
        ComponentTypeId type = 0;
        if (!ReadScalar(input, offset, type)) return std::nullopt;
        snapshot.componentTypes.push_back(type);
    }
    if (offset != input.size()) return std::nullopt;
    return snapshot;
}

} // namespace

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
: _pimpl(std::make_unique<Impl>())
, _config(config)
{
}

RedisDatabase::~RedisDatabase() {
    Shutdown();
}

bool RedisDatabase::Initialize() {
    SAGA_PROFILE_SCOPE("Redis::Initialize");

    if (IsHealthy()) return true;
    _pimpl->shutdown.store(false, std::memory_order_release);
    _pimpl->redis.reset();

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
        _pimpl->redis.reset();
        LOG_ERROR("Redis", "Initialization failed: %s", e.what());
        return false;
    }
}

void RedisDatabase::Shutdown() {
    SAGA_PROFILE_SCOPE("Redis::Shutdown");

    _isHealthy.store(false, std::memory_order_release);
    _pimpl->shutdown.store(true, std::memory_order_release);
    _pimpl->queueCV.notify_all();
    
    if (_pimpl->workerThread.joinable()) {
        _pimpl->workerThread.join();
    }
    
    _pimpl->redis.reset();

    LOG_INFO("Redis", "Shutdown (writes=%llu, errors=%llu)", 
             _totalWrites.load(), _totalErrors.load());
}

void RedisDatabase::WriteEntity(const EntitySnapshot& snapshot, DatabaseCallback cb) {
    SAGA_PROFILE_SCOPE("Redis::WriteEntity");
    std::string key = "entity:" + std::to_string(snapshot.entityId);
    SetCache(key, EncodeSnapshot(snapshot), std::move(cb));
}

void RedisDatabase::ReadEntity(EntityId entityId, EntityReadCallback cb) {
    SAGA_PROFILE_SCOPE("Redis::ReadEntity");
    std::string key = "entity:" + std::to_string(entityId);
    GetCache(key, [cb = std::move(cb)](DatabaseStatus status,
                                      std::optional<std::vector<uint8_t>> data) mutable {
        if (!status || !data) {
            if (cb) cb(std::move(status), std::nullopt);
            return;
        }
        auto snapshot = DecodeSnapshot(*data);
        if (!snapshot) {
            if (cb) cb(Failure(DatabaseError::InvalidData, "Cached entity snapshot is invalid"),
                       std::nullopt);
            return;
        }
        if (cb) cb(DatabaseStatus::Success(), std::move(snapshot));
    });
}

void RedisDatabase::DeleteEntity(EntityId entityId, DatabaseCallback cb) {
    std::string key = "entity:" + std::to_string(entityId);
    DeleteCache(key, std::move(cb));
}

void RedisDatabase::AppendEvent(const std::string& type, const std::vector<uint8_t>& data, DatabaseCallback cb) {
    if (!IsHealthy() || !_pimpl->redis) {
        if (cb) cb(Failure(DatabaseError::NotInitialized, "Redis is not initialized"));
        _totalErrors.fetch_add(1, std::memory_order_relaxed);
        return;
    }
    try {
        const std::string payload(data.begin(), data.end());
        const std::vector<std::pair<std::string, std::string>> fields{
            {"type", type}, {"data", payload}};
        _pimpl->redis->xadd("events", "*", fields.begin(), fields.end());
        _totalWrites.fetch_add(1, std::memory_order_relaxed);
        if (cb) cb(DatabaseStatus::Success());
    }
    catch (const std::exception& e) {
        LOG_ERROR("Redis", "AppendEvent failed: %s", e.what());
        _totalErrors.fetch_add(1, std::memory_order_relaxed);
        if (cb) cb(Failure(DatabaseError::BackendFailure, e.what()));
    }
}

void RedisDatabase::SetCache(const std::string& key, const std::vector<uint8_t>& value, DatabaseCallback cb) {
    if (!_isHealthy.load(std::memory_order_acquire)) {
        if (cb) cb(Failure(DatabaseError::NotInitialized, "Redis is not initialized"));
        _totalErrors.fetch_add(1, std::memory_order_relaxed);
        return;
    }
    
    if (_pimpl->pendingWrites.load(std::memory_order_acquire) >= _config.writeQueueSize) {
        if (cb) cb(Failure(DatabaseError::QueueFull, "Redis write queue is full"));
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

void RedisDatabase::GetCache(const std::string& key, DataReadCallback cb) {
    SAGA_PROFILE_SCOPE("Redis::GetCache");

    if (!IsHealthy() || !_pimpl->redis) {
        if (cb) cb(Failure(DatabaseError::NotInitialized, "Redis is not initialized"), std::nullopt);
        _totalErrors.fetch_add(1, std::memory_order_relaxed);
        return;
    }

    auto startTime = std::chrono::steady_clock::now();
    
    try {
        auto val = _pimpl->redis->get(key);
        
        auto endTime = std::chrono::steady_clock::now();
        float latency = std::chrono::duration<float, std::milli>(endTime - startTime).count();
        _pimpl->avgWriteLatencyMs.store(latency, std::memory_order_relaxed);
        _totalReads.fetch_add(1, std::memory_order_relaxed);
        
        if (!val) {
            if (cb) cb(Failure(DatabaseError::NotFound, "Key was not found"), std::nullopt);
            return;
        }
        if (cb) cb(DatabaseStatus::Success(),
                   std::vector<uint8_t>(val->begin(), val->end()));
    }
    catch (const std::exception& e) {
        LOG_ERROR("Redis", "GetCache failed: %s", e.what());
        if (cb) cb(Failure(DatabaseError::BackendFailure, e.what()), std::nullopt);
        _totalErrors.fetch_add(1, std::memory_order_relaxed);
    }
}

void RedisDatabase::DeleteCache(const std::string& key, DatabaseCallback cb) {
    if (!IsHealthy() || !_pimpl->redis) {
        if (cb) cb(Failure(DatabaseError::NotInitialized, "Redis is not initialized"));
        _totalErrors.fetch_add(1, std::memory_order_relaxed);
        return;
    }
    try {
        _pimpl->redis->del(key);
        if (cb) cb(DatabaseStatus::Success());
        _totalWrites.fetch_add(1, std::memory_order_relaxed);
    }
    catch (const std::exception& e) {
        LOG_ERROR("Redis", "DeleteCache failed: %s", e.what());
        if (cb) cb(Failure(DatabaseError::BackendFailure, e.what()));
        _totalErrors.fetch_add(1, std::memory_order_relaxed);
    }
}

void RedisDatabase::ProcessWriteQueue() {
    SAGA_PROFILE_SCOPE("Redis::WorkerThread");
    
    while (true) {
        Impl::WriteRequest request;
        
        {
            std::unique_lock<std::mutex> lock(_pimpl->queueMutex);
            _pimpl->queueCV.wait_for(lock, std::chrono::milliseconds(100), [this]() {
                return !_pimpl->writeQueue.empty() || _pimpl->shutdown.load(std::memory_order_acquire);
            });
            
            if (_pimpl->writeQueue.empty()) {
                if (_pimpl->shutdown.load(std::memory_order_acquire)) break;
                continue;
            }
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
        
        if (request.callback) {
            request.callback(success
                ? DatabaseStatus::Success()
                : Failure(DatabaseError::BackendFailure, std::move(error)));
        }
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
