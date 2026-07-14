/// @file ShardManager.h
/// @brief Shard-level zone routing, load balancing, and capacity management.
///
/// Layer  : SagaServer / Networking / Server
/// Purpose: ShardManager maintains the registry of all ZoneServer instances
///          within a shard partition. It routes incoming client sessions to the
///          least-loaded zone, enforces per-zone capacity, and handles zone
///          overflow by triggering dynamic zone spawning signals.
///
/// Threading: All public methods are thread-safe via internal shared_mutex.

#pragma once

#include "SagaServer/Networking/Core/NetworkTypes.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <optional>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace SagaServer::Networking
{

// Bring engine networking types into scope
using SagaEngine::Networking::ClientId;
using SagaEngine::Networking::ConnectionId;

// ─── ZoneDescriptor ───────────────────────────────────────────────────────────

struct ZoneDescriptor
{
    uint32_t    zoneId{0};
    uint32_t    shardId{0};
    std::string name;
    std::string bindAddress;
    uint16_t    port{0};
    uint32_t    maxClients{0};
    float       worldX{0.0f};
    float       worldY{0.0f};
    float       worldZ{0.0f};
    float       radius{1000.0f};
};

// ─── ZoneLoadInfo ─────────────────────────────────────────────────────────────

struct ZoneLoadInfo
{
    uint32_t zoneId{0};
    uint32_t currentClients{0};
    uint32_t maxClients{0};
    float    cpuUsagePct{0.0f};
    float    tickBudgetUsedPct{0.0f};
    uint64_t lastHeartbeatMs{0};
    bool     overloaded{false};
    bool     reachable{true};

    [[nodiscard]] float LoadFactor() const noexcept
    {
        if (maxClients == 0) return 1.0f;
        return static_cast<float>(currentClients) / static_cast<float>(maxClients);
    }
};

// ─── ShardConfig ──────────────────────────────────────────────────────────────

struct ShardConfig
{
    uint32_t shardId{0};
    uint32_t maxZones{64};
    uint32_t zoneHeartbeatTimeoutMs{5000};
    float    overloadThreshold{0.85f};
    bool     enableDynamicSpawn{false};
};

// ─── ShardManagerStats ────────────────────────────────────────────────────────

struct ShardManagerStats
{
    std::atomic<uint32_t> totalZonesRegistered{0};
    std::atomic<uint64_t> totalRouteDecisions{0};
    std::atomic<uint64_t> totalRoutingFailures{0};
    std::atomic<uint32_t> currentOverloadedZones{0};
    std::atomic<uint32_t> currentUnreachableZones{0};
};

// ─── Callbacks ────────────────────────────────────────────────────────────────

using OnZoneOverloadCallback    = std::function<void(uint32_t zoneId, float loadFactor)>;
using OnZoneUnreachableCallback = std::function<void(uint32_t zoneId)>;

// ─── ShardManager ─────────────────────────────────────────────────────────────

class ShardManager final
{
public:
    explicit ShardManager(ShardConfig config);
    ~ShardManager();

    ShardManager(const ShardManager&)            = delete;
    ShardManager& operator=(const ShardManager&) = delete;

    // ── Zone registry ─────────────────────────────────────────────────────────

    [[nodiscard]] bool RegisterZone(ZoneDescriptor descriptor);
    void               UnregisterZone(uint32_t zoneId);
    void               UpdateZoneLoad(uint32_t zoneId, const ZoneLoadInfo& load);

    // ── Routing ───────────────────────────────────────────────────────────────

    [[nodiscard]] std::optional<uint32_t> RouteClient(
        ClientId clientId,
        float worldX = 0.0f,
        float worldY = 0.0f,
        float worldZ = 0.0f) const;

    [[nodiscard]] std::optional<uint32_t> GetClientZone(ClientId clientId) const;
    void AssignClientToZone(ClientId clientId, uint32_t zoneId);
    void UnassignClient(ClientId clientId);

    // ── Queries ───────────────────────────────────────────────────────────────

    [[nodiscard]] uint32_t                    GetShardId()      const noexcept;
    [[nodiscard]] std::vector<ZoneDescriptor> GetAllZones()     const;
    [[nodiscard]] std::vector<ZoneLoadInfo>   GetAllLoadInfos() const;
    [[nodiscard]] std::optional<ZoneLoadInfo> GetZoneLoad(uint32_t zoneId) const;
    [[nodiscard]] uint32_t                    GetZoneCount()    const noexcept;
    [[nodiscard]] uint32_t                    GetClientCount()  const noexcept;
    [[nodiscard]] const ShardManagerStats&    GetStats()        const noexcept;

    // ── Health ────────────────────────────────────────────────────────────────

    /// Mark stale zones unreachable. Call periodically from a maintenance thread.
    void CheckZoneHealth();

    // ── Callbacks ─────────────────────────────────────────────────────────────

    void SetOnZoneOverload(OnZoneOverloadCallback cb);
    void SetOnZoneUnreachable(OnZoneUnreachableCallback cb);

private:
    [[nodiscard]] float ScoreZoneForClient(const ZoneDescriptor& desc,
                                           const ZoneLoadInfo&   load,
                                           float worldX,
                                           float worldY,
                                           float worldZ) const noexcept;

    ShardConfig         m_config;
    mutable ShardManagerStats  m_stats;

    mutable std::shared_mutex m_mutex;

    std::unordered_map<uint32_t, ZoneDescriptor> m_zones;
    std::unordered_map<uint32_t, ZoneLoadInfo>   m_loadInfos;
    std::unordered_map<ClientId, uint32_t>       m_clientZone;

    OnZoneOverloadCallback    m_onZoneOverload;
    OnZoneUnreachableCallback m_onZoneUnreachable;
};

} // namespace SagaServer::Networking
