#pragma once
#include <Persistence/IDatabase.h>
#include <memory>
#include <atomic>
#include <string>
#include <optional>

namespace SagaEngine::Persistence {

struct RedisConfig {
    std::string host{"127.0.0.1"};
    uint16_t port{6379};
    std::string password;
    uint32_t maxConnectionPool{10};
    uint32_t ttlSeconds{3600};
};

class RedisDatabase : public IDatabase {
public:
    explicit RedisDatabase(const RedisConfig& config);
    ~RedisDatabase() override;

    RedisDatabase(const RedisDatabase&) = delete;
    RedisDatabase& operator=(const RedisDatabase&) = delete;

    bool Initialize() override;
    void Shutdown() override;
    void WriteEntity(const EntitySnapshot& snapshot, DatabaseCallback cb) override;
    void ReadEntity(EntityId entityId, DatabaseCallback cb) override;
    void DeleteEntity(EntityId entityId, DatabaseCallback cb) override;
    void AppendEvent(const std::string& type, const std::vector<uint8_t>& data, DatabaseCallback cb) override;
    DatabaseStats GetStatistics() const override;
    bool IsHealthy() const override { return _isHealthy.load(std::memory_order_acquire); }

    void SetCache(const std::string& key, const std::vector<uint8_t>& value, DatabaseCallback cb);
    void GetCache(const std::string& key, DatabaseCallback cb);
    void DeleteCache(const std::string& key, DatabaseCallback cb);

private:
    struct Impl;
    std::unique_ptr<Impl> _pimpl;
    RedisConfig _config;
    std::atomic<bool> _isHealthy{false};
    std::atomic<uint64_t> _totalWrites{0};
    std::atomic<uint64_t> _totalReads{0};
    std::atomic<uint64_t> _totalErrors{0};
    
    void ProcessWriteQueue();
};

} // namespace SagaEngine::Persistence