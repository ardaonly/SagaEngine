/// @file InputCommandBuffer.cpp
/// @brief Thread-safe client-side command buffer: pending send queue + unacked window.
///
/// Correct location: Engine/src/SagaEngine/Input/Commands/InputCommandBuffer.cpp
/// DELETE the duplicate copies in Input/Devices/ and Input/Mapping/.

#include "SagaEngine/Input/Commands/InputCommandBuffer.h"
#include <algorithm>

namespace SagaEngine::Input
{

// ─── PushLocal ────────────────────────────────────────────────────────────────

bool InputCommandBuffer::PushLocal(const InputCommand& cmd)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    // Reject if the total buffered (pending + unacked) would exceed the cap.
    if (m_pending.size() + m_unacked.size() >= kMaxUnackedCommands)
        return false;

    m_pending.push_back(cmd);
    return true;
}

// ─── PeekPending ─────────────────────────────────────────────────────────────

std::vector<InputCommand> InputCommandBuffer::PeekPending() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return std::vector<InputCommand>(m_pending.begin(), m_pending.end());
}

// ─── MarkSent ─────────────────────────────────────────────────────────────────

void InputCommandBuffer::MarkSent(uint32_t upToSequence)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    while (!m_pending.empty() &&
           m_pending.front().sequence <= upToSequence)
    {
        m_unacked.push_back(m_pending.front());
        m_pending.pop_front();
    }
}

// ─── AckUpTo ─────────────────────────────────────────────────────────────────

void InputCommandBuffer::AckUpTo(uint32_t sequence)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    while (!m_unacked.empty() &&
           m_unacked.front().sequence <= sequence)
    {
        m_unacked.pop_front();
    }
}

// ─── GetUnackedSnapshot ───────────────────────────────────────────────────────

std::vector<InputCommand> InputCommandBuffer::GetUnackedSnapshot() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return std::vector<InputCommand>(m_unacked.begin(), m_unacked.end());
}

// ─── FindByTick ───────────────────────────────────────────────────────────────

std::optional<InputCommand> InputCommandBuffer::FindByTick(uint32_t tick) const
{
    std::lock_guard<std::mutex> lock(m_mutex);

    for (const auto& cmd : m_unacked)
    {
        if (cmd.simulationTick == tick)
            return cmd;
    }
    for (const auto& cmd : m_pending)
    {
        if (cmd.simulationTick == tick)
            return cmd;
    }
    return std::nullopt;
}

// ─── Diagnostics ─────────────────────────────────────────────────────────────

size_t InputCommandBuffer::GetPendingCount() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_pending.size();
}

size_t InputCommandBuffer::GetUnackedCount() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_unacked.size();
}

bool InputCommandBuffer::IsFull() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return (m_pending.size() + m_unacked.size()) >= kMaxUnackedCommands;
}

void InputCommandBuffer::Reset()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_pending.clear();
    m_unacked.clear();
}

} // namespace SagaEngine::Input