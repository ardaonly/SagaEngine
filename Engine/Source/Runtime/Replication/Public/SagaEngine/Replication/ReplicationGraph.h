/// @file ReplicationGraph.h
/// @brief Per-client entity relevancy graph with priority queues and bandwidth budgeting.
///
/// Layer  : SagaServer / Networking / Replication
/// Purpose: ReplicationGraph answers "which entities should be sent to which
///          client this tick?" combining spatial interest queries, priority
///          scoring, bandwidth budget enforcement, and hysteresis on enter/leave
///          to prevent boundary thrashing.
///
/// Threading:
///   Evaluate() and EvaluateForClient() are called from the tick thread.
///   UpdateClientPosition() may be called from a simulation thread.
///   All public methods are protected by m_mutex (shared_mutex).

#pragma once

#include "SagaServer/Networking/Interest/InterestArea.h"
#include "SagaServer/Networking/Replication/ReplicationPriority.h"
#include "SagaServer/Networking/Replication/ReplicationState.h"
#include "SagaEngine/ECS/Entity.h"
#include "SagaServer/Networking/Core/NetworkTypes.h"

#include <chrono>
#include <cstdint>
#include <optional>
#include <shared_mutex>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace SagaEngine::Networking::Replication
{

using EntityId = ECS::EntityId;
using ClientId = ::SagaEngine::Networking::ClientId;

// ─── EntityRelevancyScore ─────────────────────────────────────────────────────

struct EntityRelevancyScore
{
    EntityId entityId{0};
    float    score{0.0f};
    uint32_t ticksSinceLastSent{0};
    float    distanceMeters{0.0f};
    bool     isNew{false};
    bool     isLeaving{false};
};

// ─── ReplicationGraphConfig ───────────────────────────────────────────────────

struct ReplicationGraphConfig
{
    uint32_t maxEntitiesPerClientPerTick{64};
    float    newEntityPriorityBoost{10.0f};
    float    leavingEntityPriorityBoost{5.0f};
    float    distanceDecayRate{0.01f};
    float    stallPenaltyPerTick{0.1f};
    uint32_t hysteresisEnterDelayTicks{0};
    uint32_t hysteresisLeaveDelayTicks{2};
    uint32_t bandwidthBudgetBytesPerClient{0};  ///< 0 = uncapped
};

// ─── ClientGraphState ─────────────────────────────────────────────────────────

struct ClientGraphState
{
    ClientId clientId{0};

    std::unordered_set<EntityId>         inInterest;
    std::unordered_map<EntityId, uint32_t> enterCountdown;
    std::unordered_map<EntityId, uint32_t> leaveCountdown;
    std::unordered_map<EntityId, uint32_t> ticksSinceLastSent;

    float    playerWorldX{0.0f};
    float    playerWorldY{0.0f};
    float    playerWorldZ{0.0f};
    uint32_t ticksAlive{0};
};

// ─── ReplicationGraphOutput ───────────────────────────────────────────────────

struct ReplicationGraphOutput
{
    ClientId                          clientId{0};
    std::vector<EntityRelevancyScore> orderedEntities;  ///< Highest score first
    std::vector<EntityId>             enteredInterest;
    std::vector<EntityId>             leftInterest;
};

// ─── ReplicationGraph ─────────────────────────────────────────────────────────

class ReplicationGraph final
{
public:
    ReplicationGraph(ReplicationGraphConfig          config,
                     Interest::InterestManager*       interestManager,
                     ReplicationStateManager*         stateManager);

    ~ReplicationGraph();

    ReplicationGraph(const ReplicationGraph&)            = delete;
    ReplicationGraph& operator=(const ReplicationGraph&) = delete;

    // ── Client registration ───────────────────────────────────────────────────

    void AddClient(ClientId clientId);
    void RemoveClient(ClientId clientId);
    void UpdateClientPosition(ClientId clientId, float worldX, float worldY, float worldZ);

    // ── Graph evaluation ──────────────────────────────────────────────────────

    /// Evaluate for all clients — called once per replication tick.
    [[nodiscard]] std::vector<ReplicationGraphOutput>
        Evaluate(const std::vector<EntityId>& allEntities, uint32_t currentTick);

    /// Evaluate for a single client (fast reconnect handshake path).
    [[nodiscard]] ReplicationGraphOutput
        EvaluateForClient(ClientId                     clientId,
                          const std::vector<EntityId>& allEntities,
                          uint32_t                     currentTick);

    // ── Queries ───────────────────────────────────────────────────────────────

    [[nodiscard]] std::optional<std::unordered_set<EntityId>>
        GetClientInterestSet(ClientId clientId) const;

    [[nodiscard]] bool ClientHasEntity(ClientId clientId, EntityId entityId) const;

    // ── Config ────────────────────────────────────────────────────────────────

    void SetConfig(const ReplicationGraphConfig& config);
    [[nodiscard]] const ReplicationGraphConfig& GetConfig() const noexcept;

private:
    // ── Internal — caller must hold m_mutex ───────────────────────────────────

    [[nodiscard]] ReplicationGraphOutput EvaluateForClientLocked(
        ClientGraphState&            state,
        const std::vector<EntityId>& allEntities,
        uint32_t                     currentTick);

    [[nodiscard]] float ScoreEntity(const ClientGraphState& state,
                                    EntityId                entityId,
                                    float                   distanceMeters,
                                    bool                    isNew,
                                    bool                    isLeaving) const noexcept;

    void UpdateHysteresis(ClientGraphState&            state,
                          const std::vector<EntityId>& spatialSet,
                          std::vector<EntityId>&        outEntered,
                          std::vector<EntityId>&        outLeft);

    ReplicationGraphConfig     m_config;
    Interest::InterestManager* m_interestManager;
    ReplicationStateManager*   m_stateManager;

    mutable std::shared_mutex                        m_mutex;
    std::unordered_map<ClientId, ClientGraphState>   m_clientStates;
};

} // namespace SagaEngine::Networking::Replication
