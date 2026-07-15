/// @file MovementDirtyReplicationBridge.h
/// @brief Narrow bridge from accepted authoritative movement dirtiness to replication intent.

#pragma once

#include "SagaEngine/ServerAuthority/Simulation/AuthoritativeMovementCore.h"

#include <cstdint>
#include <cstddef>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace SagaEngine::Replication
{

struct MovementDirtyReplicationIntent
{
    SagaEngine::ServerAuthority::Simulation::EntityId entityId{0};
    std::uint64_t                            serverTick{0};
};

class MovementDirtyReplicationBridge final
{
public:
    MovementDirtyReplicationBridge() = default;
    ~MovementDirtyReplicationBridge() = default;

    MovementDirtyReplicationBridge(const MovementDirtyReplicationBridge&) = delete;
    MovementDirtyReplicationBridge& operator=(const MovementDirtyReplicationBridge&) = delete;

    void RecordMovementTick(
        const SagaEngine::ServerAuthority::Simulation::AuthoritativeMovementTickReport& report,
        std::uint64_t serverTick);

    [[nodiscard]] std::vector<MovementDirtyReplicationIntent>
        ConsumeDirtyMovementIntents();

    [[nodiscard]] std::size_t PendingCount() const;
    void Clear();

private:
    mutable std::mutex m_mutex;
    std::vector<MovementDirtyReplicationIntent> m_pending;
    std::unordered_map<SagaEngine::ServerAuthority::Simulation::EntityId, std::size_t>
        m_pendingIndexByEntity;
};

} // namespace SagaEngine::Replication
