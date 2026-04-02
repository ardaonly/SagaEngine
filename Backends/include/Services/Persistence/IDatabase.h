#pragma once
#include "Services/Persistence/Types.h"
#include <functional>
#include <string>

namespace SagaEngine::Persistence {

using DatabaseCallback = std::function<void(bool success, const std::string& error)>;
using EventCallback = std::function<void(const std::string& type, const std::vector<uint8_t>& data)>;

class IDatabase {
public:
    virtual ~IDatabase() = default;
    virtual bool Initialize() = 0;
    virtual void Shutdown() = 0;
    virtual void WriteEntity(const EntitySnapshot& snapshot, DatabaseCallback cb) = 0;
    virtual void ReadEntity(EntityId entityId, DatabaseCallback cb) = 0;
    virtual void DeleteEntity(EntityId entityId, DatabaseCallback cb) = 0;
    virtual void AppendEvent(const std::string& type, const std::vector<uint8_t>& data, DatabaseCallback cb) = 0;
    virtual DatabaseStats GetStatistics() const = 0;
    virtual bool IsHealthy() const = 0;
};

}