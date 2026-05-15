/// @file RelevanceGraph.cpp
/// @brief Context-based interest graph implementation.

#include "SagaEngine/World/RelevanceGraph.h"
#include "SagaEngine/Core/Time/Time.h"
#include <algorithm>
#include <chrono>

namespace SagaEngine::World {

using namespace std::chrono;

void RelevanceGraph::AddRule(RelevanceRule rule) noexcept
{
    m_rules.push_back(std::move(rule));
}

void RelevanceGraph::ClearRules() noexcept
{
    m_rules.clear();
}

void RelevanceGraph::SetEntityList(std::vector<EntityId> entities) noexcept
{
    m_entities = std::move(entities);
}

void RelevanceGraph::SetEdge(EntityId source, RelevanceEdge edge) noexcept
{
    auto& edges = m_adjacency[source];

    // Check if edge with same target + reason already exists.
    for (auto& e : edges)
    {
        if (e.target == edge.target && e.reason == edge.reason)
        {
            e.weight = edge.weight;
            e.context = edge.context;
            return;
        }
    }

    // Add new edge.
    edges.push_back(std::move(edge));
    m_totalEdges++;
}

void RelevanceGraph::RemoveSource(EntityId source) noexcept
{
    auto it = m_adjacency.find(source);
    if (it != m_adjacency.end())
    {
        m_totalEdges -= it->second.size();
        m_adjacency.erase(it);
    }
}

void RelevanceGraph::RemoveTarget(EntityId target) noexcept
{
    for (auto& [source, edges] : m_adjacency)
    {
        const auto before = edges.size();
        edges.erase(
            std::remove_if(edges.begin(), edges.end(),
                [target](const RelevanceEdge& e) { return e.target == target; }),
            edges.end());
        m_totalEdges -= (before - edges.size());
    }

    // Clean up empty adjacency lists.
    for (auto it = m_adjacency.begin(); it != m_adjacency.end();)
    {
        if (it->second.empty())
            it = m_adjacency.erase(it);
        else
            ++it;
    }
}

void RelevanceGraph::Rebuild() noexcept
{
    const auto start = steady_clock::now();

    // Clear existing edges.
    m_adjacency.clear();
    m_totalEdges = 0;

    // Evaluate all rules for all entity pairs.
    for (const auto& source : m_entities)
    {
        for (const auto& target : m_entities)
        {
            if (source == target)
                continue;

            for (const auto& rule : m_rules)
            {
                if (!rule.computeWeight)
                    continue;

                const float weight = rule.computeWeight(source, target);
                if (weight <= 0.0f)
                    continue;

                RelevanceEdge edge;
                edge.target = target;
                edge.weight = weight;
                edge.reason = rule.reason;

                SetEdge(source, std::move(edge));
            }
        }
    }

    const auto end = steady_clock::now();
    m_stats.rebuildTimeUs = static_cast<uint64_t>(
        duration_cast<microseconds>(end - start).count());
}

void RelevanceGraph::IncrementalUpdate(EntityId changedEntity) noexcept
{
    const auto start = steady_clock::now();

    // Remove all edges from/to the changed entity.
    RemoveSource(changedEntity);
    RemoveTarget(changedEntity);

    // Re-evaluate rules for this entity as source.
    for (const auto& target : m_entities)
    {
        if (changedEntity == target)
            continue;

        for (const auto& rule : m_rules)
        {
            if (!rule.computeWeight)
                continue;

            const float weight = rule.computeWeight(changedEntity, target);
            if (weight <= 0.0f)
                continue;

            RelevanceEdge edge;
            edge.target = target;
            edge.weight = weight;
            edge.reason = rule.reason;

            SetEdge(changedEntity, std::move(edge));
        }
    }

    // Re-evaluate rules for other entities pointing to this entity.
    for (const auto& source : m_entities)
    {
        if (source == changedEntity)
            continue;

        for (const auto& rule : m_rules)
        {
            if (!rule.computeWeight)
                continue;

            const float weight = rule.computeWeight(source, changedEntity);
            if (weight <= 0.0f)
                continue;

            RelevanceEdge edge;
            edge.target = changedEntity;
            edge.weight = weight;
            edge.reason = rule.reason;

            SetEdge(source, std::move(edge));
        }
    }

    const auto end = steady_clock::now();
    m_stats.incrementalTimeUs = static_cast<uint64_t>(
        duration_cast<microseconds>(end - start).count());
}

std::vector<RelevanceResult> RelevanceGraph::Query(EntityId source,
                                                      float minWeight) const noexcept
{
    const_cast<GraphStats&>(m_stats).queriesThisTick++;

    auto it = m_adjacency.find(source);
    if (it == m_adjacency.end())
        return {};

    // Aggregate by target: max weight across all reasons.
    std::unordered_map<EntityId, RelevanceResult> best;

    for (const auto& edge : it->second)
    {
        if (edge.weight < minWeight)
            continue;

        auto& r = best[edge.target];
        r.entityId = edge.target;

        if (edge.weight > r.maxWeight)
        {
            r.maxWeight = edge.weight;
            r.topReason = edge.reason;
        }
    }

    std::vector<RelevanceResult> results;
    results.reserve(best.size());
    for (auto& [_, r] : best)
        results.push_back(std::move(r));

    // Sort by weight descending.
    std::sort(results.begin(), results.end(),
        [](const RelevanceResult& a, const RelevanceResult& b) {
            return a.maxWeight > b.maxWeight;
        });

    return results;
}

float RelevanceGraph::GetWeight(EntityId source, EntityId target) const noexcept
{
    auto it = m_adjacency.find(source);
    if (it == m_adjacency.end())
        return 0.0f;

    float maxW = 0.0f;
    for (const auto& edge : it->second)
    {
        if (edge.target == target && edge.weight > maxW)
            maxW = edge.weight;
    }

    return maxW;
}

std::vector<RelevanceEdge> RelevanceGraph::GetEdgesFrom(EntityId source) const noexcept
{
    auto it = m_adjacency.find(source);
    if (it == m_adjacency.end())
        return {};
    return it->second;
}

} // namespace SagaEngine::World
