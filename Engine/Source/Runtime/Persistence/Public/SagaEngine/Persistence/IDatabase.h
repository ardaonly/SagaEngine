#pragma once
#include "SagaEngine/Persistence/Types.h"
#include <functional>
#include <optional>
#include <string>
#include <utility>

namespace SagaEngine::Persistence {

enum class DatabaseError : uint8_t {
    None = 0,
    NotInitialized,
    NotFound,
    QueueFull,
    BackendFailure,
    Cancelled,
    InvalidData,
};

struct DatabaseStatus {
    DatabaseError code{DatabaseError::None};
    std::string message;

    [[nodiscard]] bool IsSuccess() const noexcept { return code == DatabaseError::None; }
    [[nodiscard]] explicit operator bool() const noexcept { return IsSuccess(); }

    [[nodiscard]] static DatabaseStatus Success() { return {}; }
    [[nodiscard]] static DatabaseStatus Failure(DatabaseError error, std::string detail) {
        return {error, std::move(detail)};
    }
};

using DatabaseCallback = std::function<void(DatabaseStatus status)>;
using EntityReadCallback =
    std::function<void(DatabaseStatus status, std::optional<EntitySnapshot> snapshot)>;
using DataReadCallback =
    std::function<void(DatabaseStatus status, std::optional<std::vector<uint8_t>> data)>;
using EventCallback = std::function<void(const std::string& type, const std::vector<uint8_t>& data)>;

class IDatabase {
public:
    virtual ~IDatabase() = default;
    virtual bool Initialize() = 0;
    virtual void Shutdown() = 0;
    virtual void WriteEntity(const EntitySnapshot& snapshot, DatabaseCallback cb) = 0;
    virtual void ReadEntity(EntityId entityId, EntityReadCallback cb) = 0;
    virtual void DeleteEntity(EntityId entityId, DatabaseCallback cb) = 0;
    virtual void AppendEvent(const std::string& type, const std::vector<uint8_t>& data, DatabaseCallback cb) = 0;
    virtual DatabaseStats GetStatistics() const = 0;
    virtual bool IsHealthy() const = 0;
};

}
