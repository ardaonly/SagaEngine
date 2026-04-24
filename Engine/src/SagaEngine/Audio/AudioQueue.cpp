/// @file AudioQueue.cpp
/// @brief Thread-safe audio event queue — gameplay thread → audio thread.

#include "SagaEngine/Audio/AudioQueue.h"

#include <algorithm>

namespace SagaEngine::Audio
{

void AudioQueue::Push(AudioEvent event)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_pending.push_back(std::move(event));
}

void AudioQueue::PushBatch(AudioEventBatch& batch)
{
    if (batch.Empty()) return;

    std::lock_guard<std::mutex> lock(m_mutex);
    m_pending.reserve(m_pending.size() + batch.events.size());
    for (auto& e : batch.events)
        m_pending.push_back(std::move(e));
    batch.Clear();
}

std::uint32_t AudioQueue::Drain(std::vector<AudioEvent>& out)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_pending.empty()) return 0;

    std::uint32_t count = static_cast<std::uint32_t>(m_pending.size());
    out.reserve(out.size() + count);
    for (auto& e : m_pending)
        out.push_back(std::move(e));
    m_pending.clear();
    return count;
}

std::size_t AudioQueue::ApproxSize() const noexcept
{
    // No lock — approximate is fine for diagnostics.
    return m_pending.size();
}

void AudioQueue::Clear()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_pending.clear();
}

} // namespace SagaEngine::Audio
