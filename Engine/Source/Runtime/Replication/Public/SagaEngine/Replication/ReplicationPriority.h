/// @file ReplicationPriority.h
/// @brief Replication priority classes and per-entity priority assignment.
///
/// Layer  : SagaServer / Networking / Replication
/// Purpose: Entities are categorised into four priority tiers that control
///          their update frequency and bandwidth allocation during replication.
///          Critical entities (e.g. the player's own avatar, nearby combat targets)
///          are sent every tick.  Low-priority entities (e.g. distant scenery,
///          background NPCs) are sent less frequently and may be dropped first
///          when bandwidth runs out.
///
/// Integration:
///   - ReplicationGraph reads the PriorityClass of each entity while scoring.
///   - PriorityClass → base score multiplier, applied before distance/stall
///     adjustments.
///   - The per-entity priority can be set by gameplay systems via
///     EntityPriorityTable::SetPriority().

#pragma once

#include "SagaEngine/ECS/Entity.h"

#include <cstdint>
#include <mutex>
#include <unordered_map>

namespace SagaEngine::Networking::Replication
{

using EntityId = ECS::EntityId;

// ─── Priority Classes ─────────────────────────────────────────────────────────

/// Four-tier replication priority.
/// Higher numeric value = more important = sent first.
enum class PriorityClass : uint8_t
{
    Low      = 0, ///< Background scenery, distant non-interactive entities.
    Normal   = 1, ///< Default tier — most world entities.
    High     = 2, ///< Nearby NPCs, interactive objects, party members.
    Critical = 3  ///< Player's own avatar, immediate combat targets.
};

/// Convert PriorityClass enum to a human-readable C-string for logging.
[[nodiscard]] inline const char* PriorityClassToString(PriorityClass pc) noexcept
{
    switch (pc)
    {
        case PriorityClass::Low:      return "Low";
        case PriorityClass::Normal:   return "Normal";
        case PriorityClass::High:     return "High";
        case PriorityClass::Critical: return "Critical";
        default:                      return "Unknown";
    }
}

// ─── Priority Configuration ───────────────────────────────────────────────────

/// Per-class tuning parameters applied during replication scoring.
struct PriorityClassConfig
{
    float baseScoreMultiplier{1.0f};   ///< Multiplied with the computed relevancy score.
    uint32_t maxTicksBetweenUpdates{0};///< Force-send after this many ticks (0 = no force).
    bool dropOnBudgetExhaustion{true}; ///< Can this class be dropped when budget runs out?
};

/// Default tuning table for all four priority classes.
struct ReplicationPriorityConfig
{
    PriorityClassConfig critical{ 4.0f,  1, false }; ///< Every tick, never dropped.
    PriorityClassConfig high    { 2.0f,  4, false }; ///< At most every 4 ticks, not dropped.
    PriorityClassConfig normal  { 1.0f, 16, true  }; ///< At most every 16 ticks, droppable.
    PriorityClassConfig low     { 0.5f, 32, true  }; ///< At most every 32 ticks, droppable.

    /// Retrieve config for a specific priority class.
    [[nodiscard]] const PriorityClassConfig& Get(PriorityClass pc) const noexcept
    {
        switch (pc)
        {
            case PriorityClass::Critical: return critical;
            case PriorityClass::High:     return high;
            case PriorityClass::Normal:   return normal;
            case PriorityClass::Low:      return low;
            default:                      return normal;
        }
    }
};

// ─── Entity Priority Table ────────────────────────────────────────────────────

/// Thread-safe table mapping EntityId → PriorityClass.
/// Entities without an explicit assignment default to PriorityClass::Normal.
///
/// Gameplay systems (combat, proximity, quest tracking) call SetPriority()
/// to promote or demote entities each tick.  ReplicationGraph reads the
/// table during scoring via GetPriority().
class EntityPriorityTable final
{
public:
    EntityPriorityTable()  = default;
    ~EntityPriorityTable() = default;

    EntityPriorityTable(const EntityPriorityTable&)            = delete;
    EntityPriorityTable& operator=(const EntityPriorityTable&) = delete;

    // ── Mutation ──────────────────────────────────────────────────────────────

    /// Assign a priority class to an entity.
    void SetPriority(EntityId entityId, PriorityClass priority)
    {
        std::lock_guard lock(m_mutex);
        m_table[entityId] = priority;
    }

    /// Remove an entity from the table (entity destroyed).
    void Remove(EntityId entityId)
    {
        std::lock_guard lock(m_mutex);
        m_table.erase(entityId);
    }

    /// Clear all entries.
    void Clear()
    {
        std::lock_guard lock(m_mutex);
        m_table.clear();
    }

    // ── Query ─────────────────────────────────────────────────────────────────

    /// Retrieve the priority for an entity.  Returns Normal if unset.
    [[nodiscard]] PriorityClass GetPriority(EntityId entityId) const
    {
        std::lock_guard lock(m_mutex);
        const auto it = m_table.find(entityId);
        return (it != m_table.end()) ? it->second : PriorityClass::Normal;
    }

    /// Check if an entity has an explicit priority assignment.
    [[nodiscard]] bool HasEntry(EntityId entityId) const
    {
        std::lock_guard lock(m_mutex);
        return m_table.count(entityId) > 0;
    }

    /// Return the number of tracked entities.
    [[nodiscard]] std::size_t Size() const
    {
        std::lock_guard lock(m_mutex);
        return m_table.size();
    }

private:
    mutable std::mutex                               m_mutex;
    std::unordered_map<EntityId, PriorityClass>      m_table;
};

} // namespace SagaEngine::Networking::Replication
