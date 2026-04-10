/// @file BackpressureSignal.cpp
/// @brief Backpressure signaling implementation.

#include "SagaServer/Networking/Core/BackpressureSignal.h"
#include "SagaEngine/Core/Log/Log.h"

#include <algorithm>

namespace SagaEngine::Networking
{

static constexpr const char* kTag = "BackpressureSignal";

// ─── Construction ─────────────────────────────────────────────────────────────

BackpressureSignal::BackpressureSignal(BackpressureConfig config)
    : m_config(config)
{
}

// ─── Transport side (writer) ──────────────────────────────────────────────────

void BackpressureSignal::NotifyPressure(ClientId clientId,
                                         uint32_t queuedBytes,
                                         uint32_t queueCapacity)
{
    const float level = (queueCapacity > 0)
        ? std::clamp(static_cast<float>(queuedBytes) / static_cast<float>(queueCapacity),
                     0.0f, 1.0f)
        : 0.0f;

    OnBackpressureFn listenerCopy;

    {
        std::lock_guard lock(m_mutex);

        auto& state         = m_state[clientId];
        const float oldLevel = state.level;

        state.clientId      = clientId;
        state.level         = level;
        state.queuedBytes   = queuedBytes;
        state.queueCapacity = queueCapacity;

        // ── Fire listener only when crossing a threshold boundary ─────────────

        const bool crossedWarn      = (oldLevel < m_config.warnThreshold)      && (level >= m_config.warnThreshold);
        const bool crossedCritical  = (oldLevel < m_config.criticalThreshold)  && (level >= m_config.criticalThreshold);
        const bool crossedEmergency = (oldLevel < m_config.emergencyThreshold) && (level >= m_config.emergencyThreshold);
        const bool recovered        = (oldLevel >= m_config.warnThreshold)     && (level < m_config.warnThreshold);

        if (crossedWarn || crossedCritical || crossedEmergency || recovered)
        {
            if (m_listener)
                listenerCopy = m_listener;
        }
    }

    // Invoke outside the lock to avoid re-entrant deadlock.
    if (listenerCopy)
    {
        listenerCopy(clientId, level);

        if (level >= m_config.emergencyThreshold)
        {
            LOG_WARN(kTag, "Client %llu EMERGENCY backpressure (%.1f%%)",
                     static_cast<unsigned long long>(clientId), level * 100.0f);
        }
        else if (level >= m_config.criticalThreshold)
        {
            LOG_WARN(kTag, "Client %llu critical backpressure (%.1f%%)",
                     static_cast<unsigned long long>(clientId), level * 100.0f);
        }
    }
}

// ─── Replication side (reader) ────────────────────────────────────────────────

void BackpressureSignal::SetListener(OnBackpressureFn fn)
{
    std::lock_guard lock(m_mutex);
    m_listener = std::move(fn);
}

float BackpressureSignal::GetPressure(ClientId clientId) const
{
    std::lock_guard lock(m_mutex);
    const auto it = m_state.find(clientId);
    return (it != m_state.end()) ? it->second.level : 0.0f;
}

ClientPressure BackpressureSignal::GetClientPressure(ClientId clientId) const
{
    std::lock_guard lock(m_mutex);
    const auto it = m_state.find(clientId);
    if (it != m_state.end())
        return it->second;

    ClientPressure empty;
    empty.clientId = clientId;
    return empty;
}

bool BackpressureSignal::IsUnderPressure(ClientId clientId) const
{
    return GetPressure(clientId) >= m_config.warnThreshold;
}

bool BackpressureSignal::IsEmergency(ClientId clientId) const
{
    return GetPressure(clientId) >= m_config.emergencyThreshold;
}

// ─── Lifecycle ────────────────────────────────────────────────────────────────

void BackpressureSignal::RemoveClient(ClientId clientId)
{
    std::lock_guard lock(m_mutex);
    m_state.erase(clientId);
}

void BackpressureSignal::Clear()
{
    std::lock_guard lock(m_mutex);
    m_state.clear();
}

// ─── Config ───────────────────────────────────────────────────────────────────

void BackpressureSignal::SetConfig(const BackpressureConfig& config) noexcept
{
    std::lock_guard lock(m_mutex);
    m_config = config;
}

} // namespace SagaEngine::Networking
