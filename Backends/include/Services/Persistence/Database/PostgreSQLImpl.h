// Backends/include/Persistence/Database/PostgreSQLImpl.h
#pragma once
#include <Persistence/IDatabase.h>
#include <SagaEngine/Core/Threading/JobSystem.h>
#include <memory>
#include <atomic>

namespace SagaEngine::Persistence {

class PostgreSQLDatabase : public IDatabase {
public:
    explicit PostgreSQLDatabase(const PersistenceConfig& config);
    ~PostgreSQLDatabase() override;
    
    PostgreSQLDatabase(const PostgreSQLDatabase&) = delete;
    PostgreSQLDatabase& operator=(const PostgreSQLDatabase&) = delete;
    
    bool Initialize() override;
    void Shutdown() override;
    void WriteEntity(const EntitySnapshot& snapshot, DatabaseCallback cb) override;
    void ReadEntity(EntityId entityId, DatabaseCallback cb) override;
    void DeleteEntity(EntityId entityId, DatabaseCallback cb) override;
    void AppendEvent(const std::string& type, const std::vector<uint8_t>& data, DatabaseCallback cb) override;
    DatabaseStats GetStatistics() const override;
    bool IsHealthy() const override { return _isHealthy.load(std::memory_order_acquire); }
    
    void ClearTestData(EntityId minId = 1000, EntityId maxId = 10000);
    
private:
    struct Impl;
    std::unique_ptr<Impl> _pimpl;
    PersistenceConfig _config;
    std::atomic<bool> _isHealthy{false};
    std::atomic<uint64_t> _totalWrites{0};
    std::atomic<uint64_t> _totalReads{0};
    std::atomic<uint64_t> _totalErrors{0};
    
    void ProcessWriteQueue();
};

} // namespace SagaEngine::Persistence