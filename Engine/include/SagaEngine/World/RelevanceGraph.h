/// @file RelevanceGraph.h
/// @brief Context-based interest graph — relevance ≠ distance.
///
/// Layer  : SagaEngine / World
/// Purpose: Classical MMOs use distance-only interest: "if entity is
///          within X metres of the player, send updates."  This breaks
///          down for:
///
///            - Long-range snipers targeting you (far but relevant)
///            - Guild leaders in other cities (globally important)
///            - Raid members across zone boundaries (context matters)
///            - Economic rivals (market relevance, not spatial)
///            - Narrative triggers (story context, not position)
///
///          The RelevanceGraph models interest as a directed graph where
///          edges represent "A cares about B" with a weight and a reason.
///          Replication uses this graph instead of spatial proximity.
///
/// Design rules:
///   - Edges are directed: A → B means "A receives updates about B"
///   - Each edge has a weight (0.0–1.0) and a reason tag
///   - Reasons: Spatial, CombatTarget, RaidMember, GuildRival, Economy, Narrative
///   - Edge weights are summed per reason; max weight wins for replication
///   - The graph is updated every domain tick by relevance rules
///   - Queries return entities sorted by relevance weight
///
/// What this is NOT:
///   - Not a spatial index.  Spatial edges are ONE type of edge.
///   - Not a social graph.  Combat and narrative edges exist too.
///   - Not static.  Edges are added/removed every tick based on context.

#pragma once

#include "SagaEngine/ECS/Entity.h"
#include "SagaEngine/World/SimCell.h"

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace SagaEngine::World {

using ECS::EntityId;

// ─── Relevance reasons ───────────────────────────────────────────────────────────

enum class RelevanceReason : uint8_t
{
    Spatial,         ///< Within spatial range (distance-based).
    CombatTarget,    ///< Entity is targeting me or I'm targeting it.
    RaidMember,      ///< Same raid/party instance.
    GuildRelation,   ///< Same guild, rival guild, guild leader.
    Economy,         ///< Market rival, trade partner, auction watcher.
    Narrative,       ///< Story-relevant NPC, quest giver, event participant.
    GlobalInterest,  ///< World boss, server-wide event, famous player.
    Custom,          ///< Application-specific reason (max 256 custom reasons).
};

[[nodiscard]] inline const char* RelevanceReasonToString(RelevanceReason r) noexcept
{
    switch (r)
    {
        case RelevanceReason::Spatial:        return "Spatial";
        case RelevanceReason::CombatTarget:   return "CombatTarget";
        case RelevanceReason::RaidMember:     return "RaidMember";
        case RelevanceReason::GuildRelation:  return "GuildRelation";
        case RelevanceReason::Economy:        return "Economy";
        case RelevanceReason::Narrative:      return "Narrative";
        case RelevanceReason::GlobalInterest: return "GlobalInterest";
        case RelevanceReason::Custom:         return "Custom";
    }
    return "Unknown";
}

// ─── Relevance edge ─────────────────────────────────────────────────────────────

struct RelevanceEdge
{
    EntityId        target   = 0;     ///< The entity being observed.
    float           weight   = 0.0f;  ///< 0.0 (irrelevant) to 1.0 (critical).
    RelevanceReason reason   = RelevanceReason::Spatial;
    uint32_t        context  = 0;     ///< Reason-specific context (raid ID, guild ID, etc.).
};

// ─── Relevance query result ─────────────────────────────────────────────────────

struct RelevanceResult
{
    EntityId entityId = 0;
    float    maxWeight = 0.0f;         ///< Highest weight across all reasons.
    RelevanceReason topReason = RelevanceReason::Spatial; ///< Reason for the highest weight.
};

// ─── Edge rule ──────────────────────────────────────────────────────────────────

/// A rule that determines whether an edge should exist between two entities.
/// Rules are evaluated every relevance tick.
struct RelevanceRule
{
    RelevanceReason reason = RelevanceReason::Spatial;

    /// Determine the relevance weight from source to target.
    /// Returns 0.0 if no edge should exist.
    std::function<float(EntityId source, EntityId target)> computeWeight;
};

// ─── Relevance graph ────────────────────────────────────────────────────────────

/// Directed graph of entity relevance relationships.
///
/// Usage:
///   1. WorldNode creates a RelevanceGraph
///   2. Register rules (spatial, combat, guild, etc.)
///   3. Every relevance tick, call Rebuild() or IncrementalUpdate()
///   4. Query relevant entities for each client entity
///
/// Thread model: single-threaded, called from the WorldNode's simulation thread.
class RelevanceGraph
{
public:
    RelevanceGraph() = default;
    ~RelevanceGraph() = default;

    RelevanceGraph(const RelevanceGraph&)            = delete;
    RelevanceGraph& operator=(const RelevanceGraph&) = delete;

    // ─── Rule registration ────────────────────────────────────────────────────

    /// Register a relevance rule.  Rules are evaluated during Rebuild().
    void AddRule(RelevanceRule rule) noexcept;

    /// Remove all rules (for hot reload).
    void ClearRules() noexcept;

    // ─── Edge management ──────────────────────────────────────────────────────

    /// Add or update a single edge.  If an edge with the same (source, target,
    /// reason) already exists, its weight is updated.
    void SetEdge(EntityId source, RelevanceEdge edge) noexcept;

    /// Remove all edges from a source entity.
    void RemoveSource(EntityId source) noexcept;

    /// Remove all edges to a target entity.
    void RemoveTarget(EntityId target) noexcept;

    // ─── Graph rebuild ────────────────────────────────────────────────────────

    /// Full rebuild: evaluate all rules for all entity pairs.
    /// O(N²) — use IncrementalUpdate() for large worlds.
    void Rebuild() noexcept;

    /// Incremental update: only re-evaluate edges affected by a state change.
    /// Much faster than Rebuild() when only a few entities changed.
    void IncrementalUpdate(EntityId changedEntity) noexcept;

    // ─── Entity list management (for Rebuild iteration) ───────────────────────

    /// Set the entity list used by Rebuild() for all-pairs evaluation.
    /// Called by WorldNode each relevance tick.
    void SetEntityList(std::vector<EntityId> entities) noexcept;

    // ─── Queries ──────────────────────────────────────────────────────────────

    /// Get all entities relevant to a source entity, sorted by weight (desc).
    /// Only returns entities with weight > minWeight.
    [[nodiscard]] std::vector<RelevanceResult>
        Query(EntityId source, float minWeight = 0.01f) const noexcept;

    /// Get the relevance weight for a specific (source, target) pair.
    /// Returns the max weight across all reasons.
    [[nodiscard]] float GetWeight(EntityId source, EntityId target) const noexcept;

    /// Get all edges from a source entity.
    [[nodiscard]] std::vector<RelevanceEdge> GetEdgesFrom(EntityId source) const noexcept;

    // ─── Stats ────────────────────────────────────────────────────────────────

    [[nodiscard]] uint64_t TotalEdges()     const noexcept { return m_totalEdges; }
    [[nodiscard]] uint64_t RuleCount()      const noexcept { return m_rules.size(); }
    [[nodiscard]] uint64_t SourceCount()    const noexcept { return m_adjacency.size(); }

    struct GraphStats
    {
        uint64_t rebuildTimeUs     = 0;  ///< Time for last full Rebuild() in microseconds.
        uint64_t incrementalTimeUs = 0;  ///< Time for last IncrementalUpdate() in microseconds.
        uint64_t queriesThisTick   = 0;  ///< Number of queries this tick.
    };

    [[nodiscard]] const GraphStats& GetStats() const noexcept { return m_stats; }

private:
    // Adjacency list: source → list of edges.
    std::unordered_map<EntityId, std::vector<RelevanceEdge>> m_adjacency;

    // Registered rules.
    std::vector<RelevanceRule> m_rules;

    // Edge count (for stats).
    uint64_t m_totalEdges = 0;

    // Performance stats.
    GraphStats m_stats{};

    // Entity list (for Rebuild iteration).
    std::vector<EntityId> m_entities;
};

} // namespace SagaEngine::World
