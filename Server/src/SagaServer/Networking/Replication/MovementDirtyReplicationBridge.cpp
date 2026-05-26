/// @file MovementDirtyReplicationBridge.cpp
/// @brief MovementDirtyReplicationBridge implementation.

#include "SagaServer/Networking/Replication/MovementDirtyReplicationBridge.h"

namespace SagaEngine::Networking::Replication
{

void MovementDirtyReplicationBridge::RecordMovementTick(
    const SagaEngine::Server::Simulation::AuthoritativeMovementTickReport& report,
    std::uint64_t serverTick)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    for (const auto entityId : report.dirtyEntityIds)
    {
        if (entityId == 0)
            continue;

        const auto indexIt = m_pendingIndexByEntity.find(entityId);
        if (indexIt != m_pendingIndexByEntity.end())
        {
            m_pending[indexIt->second].serverTick = serverTick;
            continue;
        }

        m_pendingIndexByEntity.emplace(entityId, m_pending.size());
        m_pending.push_back(MovementDirtyReplicationIntent{entityId, serverTick});
    }
}

std::vector<MovementDirtyReplicationIntent>
MovementDirtyReplicationBridge::ConsumeDirtyMovementIntents()
{
    std::lock_guard<std::mutex> lock(m_mutex);

    std::vector<MovementDirtyReplicationIntent> out = std::move(m_pending);
    m_pending.clear();
    m_pendingIndexByEntity.clear();
    return out;
}

std::size_t MovementDirtyReplicationBridge::PendingCount() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_pending.size();
}

void MovementDirtyReplicationBridge::Clear()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_pending.clear();
    m_pendingIndexByEntity.clear();
}

} // namespace SagaEngine::Networking::Replication
