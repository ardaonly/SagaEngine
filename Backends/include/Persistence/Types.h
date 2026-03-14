#pragma once
#include <cstdint>
#include <vector>
#include <string>

namespace SagaEngine::Persistence {

using EntityId = uint32_t;
using ComponentTypeId = uint32_t;

struct EntitySnapshot {
    EntityId entityId{0};
    uint64_t version{0};
    uint64_t timestamp{0};
    std::vector<uint8_t> data;
    std::vector<ComponentTypeId> componentTypes;
};

struct PersistenceConfig {
    std::string connectionString;
    uint32_t maxConnectionPool{10};
    uint32_t writeQueueSize{4096};
    uint32_t snapshotIntervalSeconds{300};
    bool enableWal{true};
};

struct DatabaseStats {
    uint64_t totalWrites{0};
    uint64_t totalReads{0};
    uint64_t totalErrors{0};
    float averageWriteLatencyMs{0.0f};
    uint32_t pendingWrites{0};
};

}