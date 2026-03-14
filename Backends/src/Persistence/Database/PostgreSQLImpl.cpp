#include <Persistence/Database/PostgreSQLImpl.h>
#include <SagaEngine/Core/Log/Log.h>
#include <SagaEngine/Core/Profiling/Profiler.h>
#include <pqxx/pqxx>
#include <queue>
#include <thread>
#include <chrono>
#include <mutex>
#include <condition_variable>

namespace SagaEngine::Persistence {

struct PostgreSQLDatabase::Impl {
    struct WriteRequest {
        EntitySnapshot snapshot;
        DatabaseCallback callback;
        uint64_t enqueueTime;
    };

    std::vector<std::unique_ptr<pqxx::connection>> connectionPool;
    std::mutex poolMutex;
    
    std::queue<WriteRequest> writeQueue;
    std::mutex queueMutex;
    std::condition_variable queueCV;
    std::thread workerThread;
    std::atomic<bool> shutdown{false};
    
    std::atomic<uint32_t> pendingWrites{0};
    std::atomic<float> avgWriteLatencyMs{0.0f};

    bool InitializeSchema(pqxx::connection& conn) {
        try {
            pqxx::work txn{conn};
            
            txn.exec0(R"(
                CREATE TABLE IF NOT EXISTS entities (
                    entity_id BIGINT PRIMARY KEY,
                    version BIGINT NOT NULL DEFAULT 1,
                    created_at TIMESTAMPTZ DEFAULT NOW(),
                    updated_at TIMESTAMPTZ DEFAULT NOW(),
                    is_deleted BOOLEAN NOT NULL DEFAULT FALSE
                )
            )");
            
            txn.exec0(R"(
                CREATE TABLE IF NOT EXISTS components (
                    id BIGSERIAL PRIMARY KEY,
                    entity_id BIGINT NOT NULL REFERENCES entities(entity_id) ON DELETE CASCADE,
                    component_type INTEGER NOT NULL,
                    component_data BYTEA NOT NULL,
                    version BIGINT NOT NULL DEFAULT 1,
                    created_at TIMESTAMPTZ DEFAULT NOW(),
                    UNIQUE(entity_id, component_type)
                )
            )");
            
            txn.exec0(R"(
                CREATE TABLE IF NOT EXISTS event_log (
                    event_id BIGSERIAL PRIMARY KEY,
                    event_type VARCHAR(255) NOT NULL,
                    event_data BYTEA NOT NULL,
                    timestamp TIMESTAMPTZ DEFAULT NOW(),
                    entity_id BIGINT
                )
            )");
            
            txn.exec0(R"(
                CREATE INDEX IF NOT EXISTS idx_components_entity ON components(entity_id)
            )");
            
            txn.exec0(R"(
                CREATE INDEX IF NOT EXISTS idx_event_log_timestamp ON event_log(timestamp)
            )");
            
            txn.commit();
            return true;
        }
        catch (const std::exception& e) {
            LOG_ERROR("PostgreSQL", "Schema init failed: %s", e.what());
            return false;
        }
    }
};

PostgreSQLDatabase::PostgreSQLDatabase(const PersistenceConfig& config)
: _config(config)
, _pimpl(std::make_unique<Impl>())
{
}

PostgreSQLDatabase::~PostgreSQLDatabase() {
    Shutdown();
}

bool PostgreSQLDatabase::Initialize() {
    SAGA_PROFILE_SCOPE("PostgreSQL::Initialize");
    
    try {
        for (uint32_t i = 0; i < _config.maxConnectionPool; ++i) {
            auto conn = std::make_unique<pqxx::connection>(_config.connectionString.c_str());
            if (conn->is_open()) {
                _pimpl->connectionPool.push_back(std::move(conn));
            }
        }
        
        if (_pimpl->connectionPool.empty()) {
            LOG_ERROR("PostgreSQL", "Failed to create database connections");
            return false;
        }
        
        if (!_pimpl->InitializeSchema(*_pimpl->connectionPool[0])) {
            return false;
        }
        
        _pimpl->workerThread = std::thread([this]() {
            ProcessWriteQueue();
        });
        
        _isHealthy.store(true, std::memory_order_release);
        LOG_INFO("PostgreSQL", "Initialized (pool=%zu)", _pimpl->connectionPool.size());
        return true;
    }
    catch (const std::exception& e) {
        LOG_ERROR("PostgreSQL", "Initialization failed: %s", e.what());
        return false;
    }
}

void PostgreSQLDatabase::Shutdown() {
    SAGA_PROFILE_SCOPE("PostgreSQL::Shutdown");
    
    _pimpl->shutdown.store(true, std::memory_order_release);
    _pimpl->queueCV.notify_all();
    
    if (_pimpl->workerThread.joinable()) {
        _pimpl->workerThread.join();
    }
    
    _pimpl->connectionPool.clear();
    _isHealthy.store(false, std::memory_order_release);
    
    LOG_INFO("PostgreSQL", "Shutdown (writes=%llu, errors=%llu)", 
             _totalWrites.load(), _totalErrors.load());
}

void PostgreSQLDatabase::WriteEntity(const EntitySnapshot& snapshot, DatabaseCallback cb) {
    SAGA_PROFILE_SCOPE("PostgreSQL::WriteEntity");
    
    if (!_isHealthy.load(std::memory_order_acquire)) {
        if (cb) cb(false, "Database not healthy");
        _totalErrors.fetch_add(1, std::memory_order_relaxed);
        return;
    }
    
    if (_pimpl->pendingWrites.load(std::memory_order_acquire) >= _config.writeQueueSize) {
        LOG_WARN("PostgreSQL", "Write queue full");
        if (cb) cb(false, "Queue full");
        _totalErrors.fetch_add(1, std::memory_order_relaxed);
        return;
    }
    
    {
        std::lock_guard<std::mutex> lock(_pimpl->queueMutex);
        Impl::WriteRequest request;
        request.snapshot = snapshot;
        request.callback = std::move(cb);
        request.enqueueTime = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
        _pimpl->writeQueue.push(std::move(request));
    }
    
    _pimpl->pendingWrites.fetch_add(1, std::memory_order_relaxed);
    _pimpl->queueCV.notify_one();
}

void PostgreSQLDatabase::ReadEntity(EntityId entityId, DatabaseCallback cb) {
    SAGA_PROFILE_SCOPE("PostgreSQL::ReadEntity");
    
    auto startTime = std::chrono::steady_clock::now();
    
    try {
        std::lock_guard<std::mutex> lock(_pimpl->poolMutex);
        if (_pimpl->connectionPool.empty()) {
            if (cb) cb(false, "No connections");
            _totalErrors.fetch_add(1, std::memory_order_relaxed);
            return;
        }
        
        pqxx::work txn{*_pimpl->connectionPool[0]};
        
        pqxx::result result = txn.exec_params0(
            "SELECT entity_id, version FROM entities WHERE entity_id = $1 AND is_deleted = FALSE",
            static_cast<int64_t>(entityId)
        );
        
        auto endTime = std::chrono::steady_clock::now();
        float latency = std::chrono::duration<float, std::milli>(endTime - startTime).count();
        _pimpl->avgWriteLatencyMs.store(latency, std::memory_order_relaxed);
        _totalReads.fetch_add(1, std::memory_order_relaxed);
        
        if (cb) cb(result.empty() ? false : true, result.empty() ? "Not found" : "");
    }
    catch (const std::exception& e) {
        LOG_ERROR("PostgreSQL", "Read failed: %s", e.what());
        if (cb) cb(false, e.what());
        _totalErrors.fetch_add(1, std::memory_order_relaxed);
    }
}

void PostgreSQLDatabase::DeleteEntity(EntityId entityId, DatabaseCallback cb) {
    WriteEntity({entityId, 0, 0, {}, {}}, cb);
}

void PostgreSQLDatabase::AppendEvent(const std::string& type, const std::vector<uint8_t>& data, DatabaseCallback cb) {
    EntitySnapshot snapshot;
    snapshot.entityId = 0;
    snapshot.data = data;
    snapshot.componentTypes.push_back(0);
    WriteEntity(snapshot, cb);
}

void PostgreSQLDatabase::ProcessWriteQueue() {
    SAGA_PROFILE_SCOPE("PostgreSQL::WorkerThread");
    
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
            std::lock_guard<std::mutex> lock(_pimpl->poolMutex);
            if (_pimpl->connectionPool.empty()) {
                error = "No connections";
            } else {
                pqxx::work txn{*_pimpl->connectionPool[0]};
                
                txn.exec_params0(R"(
                    INSERT INTO entities (entity_id, version, updated_at)
                    VALUES ($1, $2, NOW())
                    ON CONFLICT (entity_id) DO UPDATE SET version = $2, updated_at = NOW()
                )", 
                    static_cast<int64_t>(request.snapshot.entityId),
                    static_cast<int64_t>(request.snapshot.version)
                );
                
                for (size_t i = 0; i < request.snapshot.componentTypes.size(); ++i) {
                    txn.exec_params0(R"(
                        INSERT INTO components (entity_id, component_type, component_data, version)
                        VALUES ($1, $2, $3, $4)
                        ON CONFLICT (entity_id, component_type)
                        DO UPDATE SET component_data = $3, version = $4
                    )", 
                        static_cast<int64_t>(request.snapshot.entityId),
                        static_cast<int>(request.snapshot.componentTypes[i]),
                        pqxx::binarystring(request.snapshot.data.data(), request.snapshot.data.size()),
                        static_cast<int64_t>(request.snapshot.version)
                    );
                }
                
                txn.commit();
                success = true;
            }
        }
        catch (const std::exception& e) {
            error = e.what();
            LOG_ERROR("PostgreSQL", "Write failed: %s", e.what());
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

DatabaseStats PostgreSQLDatabase::GetStatistics() const {
    DatabaseStats stats;
    stats.totalWrites = _totalWrites.load(std::memory_order_acquire);
    stats.totalReads = _totalReads.load(std::memory_order_acquire);
    stats.totalErrors = _totalErrors.load(std::memory_order_acquire);
    stats.averageWriteLatencyMs = _pimpl->avgWriteLatencyMs.load(std::memory_order_acquire);
    stats.pendingWrites = _pimpl->pendingWrites.load(std::memory_order_acquire);
    return stats;
}

}