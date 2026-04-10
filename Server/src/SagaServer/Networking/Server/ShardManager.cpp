/// @file ShardManager.cpp
/// @brief ShardManager implementation.

#include "SagaServer/Networking/Server/ShardManager.h"
#include "SagaEngine/Core/Log/Log.h"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cmath>
#include <limits>

namespace SagaServer::Networking
{

static constexpr const char* kTag = "ShardManager";

// ─── Construction ─────────────────────────────────────────────────────────────

ShardManager::ShardManager(ShardConfig config)
    : m_config(config)
{
    m_zones.reserve(m_config.maxZones);
    m_loadInfos.reserve(m_config.maxZones);
    m_clientZone.reserve(1024);
}

ShardManager::~ShardManager() = default;

// ─── Zone registry ────────────────────────────────────────────────────────────

bool ShardManager::RegisterZone(ZoneDescriptor descriptor)
{
    std::unique_lock lock(m_mutex);

    if (m_zones.size() >= m_config.maxZones)
    {
        LOG_WARN(kTag, "Shard %u at max zone capacity (%u)",
                 m_config.shardId, m_config.maxZones);
        return false;
    }

    if (m_zones.count(descriptor.zoneId))
    {
        LOG_WARN(kTag, "Zone %u already registered in shard %u",
                 descriptor.zoneId, m_config.shardId);
        return false;
    }

    const uint32_t zoneId = descriptor.zoneId;

    ZoneLoadInfo load;
    load.zoneId             = zoneId;
    load.maxClients         = descriptor.maxClients;
    load.reachable          = true;
    load.lastHeartbeatMs    = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count());

    m_zones[zoneId]     = std::move(descriptor);
    m_loadInfos[zoneId] = std::move(load);

    m_stats.totalZonesRegistered.fetch_add(1, std::memory_order_relaxed);

    LOG_INFO(kTag, "Zone %u registered in shard %u (maxClients=%u)",
             zoneId, m_config.shardId, m_zones[zoneId].maxClients);
    return true;
}

void ShardManager::UnregisterZone(uint32_t zoneId)
{
    std::unique_lock lock(m_mutex);
    m_zones.erase(zoneId);
    m_loadInfos.erase(zoneId);

    for (auto it = m_clientZone.begin(); it != m_clientZone.end(); )
    {
        it = (it->second == zoneId) ? m_clientZone.erase(it) : std::next(it);
    }

    LOG_INFO(kTag, "Zone %u unregistered from shard %u", zoneId, m_config.shardId);
}

void ShardManager::UpdateZoneLoad(uint32_t zoneId, const ZoneLoadInfo& load)
{
    OnZoneOverloadCallback overloadCb;

    {
        std::unique_lock lock(m_mutex);
        auto it = m_loadInfos.find(zoneId);
        if (it == m_loadInfos.end())
            return;

        const bool wasOverloaded = it->second.overloaded;
        it->second               = load;
        it->second.overloaded    = (load.LoadFactor() >= m_config.overloadThreshold);

        if (it->second.overloaded && !wasOverloaded)
        {
            m_stats.currentOverloadedZones.fetch_add(1, std::memory_order_relaxed);
            overloadCb = m_onZoneOverload;
        }
        else if (!it->second.overloaded && wasOverloaded)
        {
            const uint32_t prev =
                m_stats.currentOverloadedZones.fetch_sub(1, std::memory_order_relaxed);
            if (prev == 0)
                m_stats.currentOverloadedZones.store(0, std::memory_order_relaxed);
        }
    }

    // Fire callback outside the lock to avoid inversion.
    if (overloadCb)
        overloadCb(zoneId, load.LoadFactor());
}

// ─── Routing ──────────────────────────────────────────────────────────────────

std::optional<uint32_t> ShardManager::RouteClient(ClientId clientId,
                                                    float    worldX,
                                                    float    worldY,
                                                    float    worldZ) const
{
    m_stats.totalRouteDecisions.fetch_add(1, std::memory_order_relaxed);

    std::shared_lock lock(m_mutex);

    uint32_t bestZone  = 0;
    float    bestScore = std::numeric_limits<float>::lowest();
    bool     found     = false;

    for (const auto& [zoneId, desc] : m_zones)
    {
        const auto loadIt = m_loadInfos.find(zoneId);
        if (loadIt == m_loadInfos.end())
            continue;

        const ZoneLoadInfo& loadInfo = loadIt->second;

        if (!loadInfo.reachable || loadInfo.overloaded)
            continue;

        if (loadInfo.currentClients >= loadInfo.maxClients)
            continue;

        const float score = ScoreZoneForClient(desc, loadInfo, worldX, worldY, worldZ);
        if (score > bestScore)
        {
            bestScore = score;
            bestZone  = zoneId;
            found     = true;
        }
    }

    if (!found)
    {
        m_stats.totalRoutingFailures.fetch_add(1, std::memory_order_relaxed);
        LOG_WARN(kTag, "No available zone for client %llu in shard %u",
                 static_cast<unsigned long long>(clientId), m_config.shardId);
        return std::nullopt;
    }

    return bestZone;
}

float ShardManager::ScoreZoneForClient(const ZoneDescriptor& desc,
                                        const ZoneLoadInfo&   load,
                                        float worldX,
                                        float worldY,
                                        float worldZ) const noexcept
{
    const float capacityScore  = 1.0f - load.LoadFactor();
    const float cpuScore       = 1.0f - std::clamp(load.cpuUsagePct / 100.0f, 0.0f, 1.0f);

    const float dx      = desc.worldX - worldX;
    const float dy      = desc.worldY - worldY;
    const float dz      = desc.worldZ - worldZ;
    const float dist    = std::sqrt(dx * dx + dy * dy + dz * dz);
    const float normDist = dist / std::max(desc.radius, 1.0f);
    const float proximityScore = 1.0f / (1.0f + normDist);

    return (capacityScore * 0.5f) + (cpuScore * 0.3f) + (proximityScore * 0.2f);
}

std::optional<uint32_t> ShardManager::GetClientZone(ClientId clientId) const
{
    std::shared_lock lock(m_mutex);
    const auto it = m_clientZone.find(clientId);
    if (it == m_clientZone.end())
        return std::nullopt;
    return it->second;
}

void ShardManager::AssignClientToZone(ClientId clientId, uint32_t zoneId)
{
    std::unique_lock lock(m_mutex);
    m_clientZone[clientId] = zoneId;

    auto loadIt = m_loadInfos.find(zoneId);
    if (loadIt != m_loadInfos.end())
        ++loadIt->second.currentClients;
}

void ShardManager::UnassignClient(ClientId clientId)
{
    std::unique_lock lock(m_mutex);
    const auto it = m_clientZone.find(clientId);
    if (it == m_clientZone.end())
        return;

    const uint32_t zoneId = it->second;
    m_clientZone.erase(it);

    auto loadIt = m_loadInfos.find(zoneId);
    if (loadIt != m_loadInfos.end() && loadIt->second.currentClients > 0)
        --loadIt->second.currentClients;
}

// ─── Health checks ────────────────────────────────────────────────────────────

void ShardManager::CheckZoneHealth()
{
    const auto nowMs = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count());

    std::vector<std::pair<uint32_t, bool>> transitions; // zoneId, becameUnreachable

    {
        std::unique_lock lock(m_mutex);

        for (auto& [zoneId, load] : m_loadInfos)
        {
            const uint64_t elapsed = nowMs - load.lastHeartbeatMs;
            const bool stale = elapsed > m_config.zoneHeartbeatTimeoutMs;

            if (stale && load.reachable)
            {
                load.reachable = false;
                m_stats.currentUnreachableZones.fetch_add(1, std::memory_order_relaxed);
                transitions.emplace_back(zoneId, true);

                LOG_WARN(kTag, "Zone %u unreachable — no heartbeat in %llu ms",
                         zoneId, static_cast<unsigned long long>(elapsed));
            }
            else if (!stale && !load.reachable)
            {
                load.reachable = true;
                const uint32_t prev =
                    m_stats.currentUnreachableZones.fetch_sub(1, std::memory_order_relaxed);
                if (prev == 0)
                    m_stats.currentUnreachableZones.store(0, std::memory_order_relaxed);
                transitions.emplace_back(zoneId, false);
            }
        }
    }

    // Fire callbacks outside the lock.
    for (const auto& [zoneId, becameUnreachable] : transitions)
    {
        if (becameUnreachable && m_onZoneUnreachable)
            m_onZoneUnreachable(zoneId);
    }
}

// ─── Queries ──────────────────────────────────────────────────────────────────

uint32_t ShardManager::GetShardId() const noexcept
{
    return m_config.shardId;
}

std::vector<ZoneDescriptor> ShardManager::GetAllZones() const
{
    std::shared_lock lock(m_mutex);
    std::vector<ZoneDescriptor> result;
    result.reserve(m_zones.size());
    for (const auto& [id, desc] : m_zones)
        result.push_back(desc);
    return result;
}

std::vector<ZoneLoadInfo> ShardManager::GetAllLoadInfos() const
{
    std::shared_lock lock(m_mutex);
    std::vector<ZoneLoadInfo> result;
    result.reserve(m_loadInfos.size());
    for (const auto& [id, load] : m_loadInfos)
        result.push_back(load);
    return result;
}

std::optional<ZoneLoadInfo> ShardManager::GetZoneLoad(uint32_t zoneId) const
{
    std::shared_lock lock(m_mutex);
    const auto it = m_loadInfos.find(zoneId);
    if (it == m_loadInfos.end())
        return std::nullopt;
    return it->second;
}

uint32_t ShardManager::GetZoneCount() const noexcept
{
    std::shared_lock lock(m_mutex);
    return static_cast<uint32_t>(m_zones.size());
}

uint32_t ShardManager::GetClientCount() const noexcept
{
    std::shared_lock lock(m_mutex);
    return static_cast<uint32_t>(m_clientZone.size());
}

const ShardManagerStats& ShardManager::GetStats() const noexcept
{
    return m_stats;
}

void ShardManager::SetOnZoneOverload(OnZoneOverloadCallback cb)
{
    std::unique_lock lock(m_mutex);
    m_onZoneOverload = std::move(cb);
}

void ShardManager::SetOnZoneUnreachable(OnZoneUnreachableCallback cb)
{
    std::unique_lock lock(m_mutex);
    m_onZoneUnreachable = std::move(cb);
}

} // namespace SagaServer::Networking
