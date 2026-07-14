/// @file ReplicationGraph.cpp
/// @brief ReplicationGraph implementation.

#include "SagaServer/Networking/Replication/ReplicationGraph.h"
#include "SagaEngine/Core/Log/Log.h"

#include <algorithm>
#include <cassert>
#include <cmath>

namespace SagaEngine::Networking::Replication
{

static constexpr const char* kTag = "ReplicationGraph";

// ─── Construction ─────────────────────────────────────────────────────────────

ReplicationGraph::ReplicationGraph(ReplicationGraphConfig    config,
                                    Interest::InterestManager* interestManager,
                                    ReplicationStateManager*  stateManager)
    : m_config(config)
    , m_interestManager(interestManager)
    , m_stateManager(stateManager)
{
    assert(interestManager && "InterestManager must not be null");
    assert(stateManager    && "ReplicationStateManager must not be null");
}

ReplicationGraph::~ReplicationGraph() = default;

// ─── Client registration ──────────────────────────────────────────────────────

void ReplicationGraph::AddClient(ClientId clientId)
{
    std::unique_lock lock(m_mutex);
    auto& state    = m_clientStates[clientId];
    state.clientId = clientId;
    state.ticksAlive = 0;
    LOG_DEBUG(kTag, "Client %llu added to replication graph",
              static_cast<unsigned long long>(clientId));
}

void ReplicationGraph::RemoveClient(ClientId clientId)
{
    std::unique_lock lock(m_mutex);
    m_clientStates.erase(clientId);
    LOG_DEBUG(kTag, "Client %llu removed from replication graph",
              static_cast<unsigned long long>(clientId));
}

void ReplicationGraph::UpdateClientPosition(ClientId clientId,
                                             float worldX,
                                             float worldY,
                                             float worldZ)
{
    std::unique_lock lock(m_mutex);
    auto it = m_clientStates.find(clientId);
    if (it == m_clientStates.end())
        return;

    it->second.playerWorldX = worldX;
    it->second.playerWorldY = worldY;
    it->second.playerWorldZ = worldZ;
}

// ─── Graph evaluation ─────────────────────────────────────────────────────────

std::vector<ReplicationGraphOutput>
ReplicationGraph::Evaluate(const std::vector<EntityId>& allEntities,
                            uint32_t                     currentTick)
{
    std::vector<ReplicationGraphOutput> outputs;

    std::unique_lock lock(m_mutex);
    outputs.reserve(m_clientStates.size());

    for (auto& [clientId, state] : m_clientStates)
    {
        outputs.push_back(EvaluateForClientLocked(state, allEntities, currentTick));
        ++state.ticksAlive;
    }

    return outputs;
}

ReplicationGraphOutput
ReplicationGraph::EvaluateForClient(ClientId                     clientId,
                                     const std::vector<EntityId>& allEntities,
                                     uint32_t                     currentTick)
{
    std::unique_lock lock(m_mutex);

    auto it = m_clientStates.find(clientId);
    if (it == m_clientStates.end())
    {
        ReplicationGraphOutput empty;
        empty.clientId = clientId;
        return empty;
    }

    return EvaluateForClientLocked(it->second, allEntities, currentTick);
}

// ─── Internal — lock must already be held ─────────────────────────────────────

ReplicationGraphOutput
ReplicationGraph::EvaluateForClientLocked(ClientGraphState&            state,
                                           const std::vector<EntityId>& allEntities,
                                           uint32_t                     currentTick)
{
    ReplicationGraphOutput output;
    output.clientId = state.clientId;

    // ── 1. Spatial query ──────────────────────────────────────────────────────

    const std::vector<EntityId> spatialSet =
        m_interestManager
            ? m_interestManager->GetVisibleEntities(state.clientId)
            : allEntities;

    // ── 2. Hysteresis ─────────────────────────────────────────────────────────

    UpdateHysteresis(state, spatialSet, output.enteredInterest, output.leftInterest);

    // ── 3. Score all entities currently in interest ───────────────────────────

    std::vector<EntityRelevancyScore> scored;
    scored.reserve(state.inInterest.size());

    for (EntityId entityId : state.inInterest)
    {
        const auto sentIt    = state.ticksSinceLastSent.find(entityId);
        const uint32_t stall = (sentIt != state.ticksSinceLastSent.end())
            ? (currentTick - sentIt->second) : currentTick;

        // ── Compute actual Euclidean distance from client to entity ────────────
        float dist = 0.0f;
        if (m_interestManager)
        {
            Interest::Vector3 entityPos;
            if (m_interestManager->GetEntityPosition(entityId, entityPos))
            {
                const float dx = entityPos.x - state.playerWorldX;
                const float dy = entityPos.y - state.playerWorldY;
                const float dz = entityPos.z - state.playerWorldZ;
                dist = std::sqrt(dx * dx + dy * dy + dz * dz);
            }
        }

        const bool isNew = std::find(
            output.enteredInterest.begin(),
            output.enteredInterest.end(), entityId) != output.enteredInterest.end();

        const bool isLeaving = std::find(
            output.leftInterest.begin(),
            output.leftInterest.end(), entityId) != output.leftInterest.end();

        EntityRelevancyScore s;
        s.entityId           = entityId;
        s.ticksSinceLastSent = stall;
        s.distanceMeters     = dist;
        s.isNew              = isNew;
        s.isLeaving          = isLeaving;
        s.score              = ScoreEntity(state, entityId, dist, isNew, isLeaving);
        s.score += static_cast<float>(stall) * m_config.stallPenaltyPerTick;

        scored.push_back(s);
    }

    // ── 4. Sort descending by score, cap at budget ────────────────────────────

    std::sort(scored.begin(), scored.end(),
        [](const EntityRelevancyScore& a, const EntityRelevancyScore& b) noexcept
        {
            return a.score > b.score;
        });

    if (scored.size() > m_config.maxEntitiesPerClientPerTick)
        scored.resize(m_config.maxEntitiesPerClientPerTick);

    // ── 5. Update last-sent tracker ───────────────────────────────────────────

    for (const auto& s : scored)
        state.ticksSinceLastSent[s.entityId] = currentTick;

    output.orderedEntities = std::move(scored);
    return output;
}

// ─── Hysteresis ───────────────────────────────────────────────────────────────

void ReplicationGraph::UpdateHysteresis(ClientGraphState&            state,
                                         const std::vector<EntityId>& spatialSet,
                                         std::vector<EntityId>&        outEntered,
                                         std::vector<EntityId>&        outLeft)
{
    const std::unordered_set<EntityId> spatialLookup(
        spatialSet.begin(), spatialSet.end());

    for (EntityId entityId : spatialSet)
    {
        if (state.inInterest.count(entityId))
        {
            state.enterCountdown.erase(entityId);
            continue;
        }

        uint32_t& countdown = state.enterCountdown[entityId];
        if (countdown >= m_config.hysteresisEnterDelayTicks)
        {
            state.inInterest.insert(entityId);
            state.enterCountdown.erase(entityId);
            outEntered.push_back(entityId);
        }
        else
        {
            ++countdown;
        }
    }

    std::vector<EntityId> toRemove;
    for (EntityId entityId : state.inInterest)
    {
        if (spatialLookup.count(entityId))
        {
            state.leaveCountdown.erase(entityId);
            continue;
        }

        uint32_t& countdown = state.leaveCountdown[entityId];
        if (countdown >= m_config.hysteresisLeaveDelayTicks)
        {
            toRemove.push_back(entityId);
            outLeft.push_back(entityId);
        }
        else
        {
            ++countdown;
        }
    }

    for (EntityId entityId : toRemove)
    {
        state.inInterest.erase(entityId);
        state.leaveCountdown.erase(entityId);
        state.ticksSinceLastSent.erase(entityId);
    }
}

// ─── Scoring ──────────────────────────────────────────────────────────────────

float ReplicationGraph::ScoreEntity(const ClientGraphState& /*state*/,
                                     EntityId                /*entityId*/,
                                     float                   distanceMeters,
                                     bool                    isNew,
                                     bool                    isLeaving) const noexcept
{
    float score = 1.0f;

    const float distPenalty = distanceMeters * m_config.distanceDecayRate;
    score = std::max(0.0f, score - distPenalty);

    if (isNew)     score += m_config.newEntityPriorityBoost;
    if (isLeaving) score += m_config.leavingEntityPriorityBoost;

    return score;
}

// ─── Queries ──────────────────────────────────────────────────────────────────

std::optional<std::unordered_set<EntityId>>
ReplicationGraph::GetClientInterestSet(ClientId clientId) const
{
    std::shared_lock lock(m_mutex);
    const auto it = m_clientStates.find(clientId);
    if (it == m_clientStates.end())
        return std::nullopt;
    return it->second.inInterest;
}

bool ReplicationGraph::ClientHasEntity(ClientId clientId, EntityId entityId) const
{
    std::shared_lock lock(m_mutex);
    const auto it = m_clientStates.find(clientId);
    if (it == m_clientStates.end())
        return false;
    return it->second.inInterest.count(entityId) > 0;
}

// ─── Config ───────────────────────────────────────────────────────────────────

void ReplicationGraph::SetConfig(const ReplicationGraphConfig& config)
{
    std::unique_lock lock(m_mutex);
    m_config = config;
}

const ReplicationGraphConfig& ReplicationGraph::GetConfig() const noexcept
{
    return m_config;
}

} // namespace SagaEngine::Networking::Replication
