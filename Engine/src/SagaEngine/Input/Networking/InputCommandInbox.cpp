/// @file InputCommandInbox.cpp

#include "SagaEngine/Input/Networking/InputCommandInbox.h"
#include <algorithm>

namespace SagaEngine::Input
{

using SagaEngine::Networking::ConnectionId;
using SagaEngine::Input::InputCommand;

InputCommandInbox::InputCommandInbox(ConnectionId connectionId)
    : m_connectionId(connectionId)
{}

// ─── PushReceived ─────────────────────────────────────────────────────────────

InputCommandInbox::PushResult
InputCommandInbox::PushReceived(const InputCommand& cmd)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    ++m_totalReceived;

    if (m_lastProcessedSequence > 0 && !IsInWindow(cmd.sequence))
    {
        ++m_droppedCount;
        return PushResult::OutOfWindow;
    }

    if (cmd.sequence <= m_lastProcessedSequence && m_lastProcessedSequence > 0)
    {
        ++m_duplicateCount;
        return PushResult::Duplicate;
    }

    for (const auto& existing : m_queue)
    {
        if (existing.sequence == cmd.sequence)
        {
            ++m_duplicateCount;
            return PushResult::Duplicate;
        }
    }

    if (m_queue.size() >= kInboxCapacity)
    {
        ++m_droppedCount;
        return PushResult::QueueFull;
    }

    InsertSorted(cmd);
    return PushResult::Accepted;
}

// ─── ConsumeForTick ───────────────────────────────────────────────────────────

InputCommandInbox::TickResult
InputCommandInbox::ConsumeForTick(uint32_t /*serverTick*/)
{
    m_commandsThisTick = 0;

    TickResult result;

    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_queue.empty())
    {
        ++m_lateCount;
        result.isLate = true;

        if (m_hasLastCommand)
        {
            result.command      = m_lastCommand;
            result.usedFallback = true;
            ++m_fallbackCount;
        }
        return result;
    }

    InputCommand cmd = m_queue.front();
    m_queue.pop_front();

    m_lastProcessedSequence = cmd.sequence;
    m_lastCommand           = cmd;
    m_hasLastCommand        = true;

    result.command = cmd;
    return result;
}

// ─── Diagnostics ─────────────────────────────────────────────────────────────

size_t InputCommandInbox::GetQueueDepth() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_queue.size();
}

void InputCommandInbox::Reset()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_queue.clear();
    m_lastCommand           = {};
    m_hasLastCommand        = false;
    m_lastProcessedSequence = 0;
    m_commandsThisTick      = 0;
    m_totalReceived         = 0;
    m_duplicateCount        = 0;
    m_droppedCount          = 0;
    m_lateCount             = 0;
    m_fallbackCount         = 0;
}

// ─── Private ─────────────────────────────────────────────────────────────────

void InputCommandInbox::InsertSorted(const InputCommand& cmd)
{
    auto it = std::lower_bound(
        m_queue.begin(), m_queue.end(), cmd,
        [](const InputCommand& a, const InputCommand& b) {
            return a.sequence < b.sequence;
        }
    );
    m_queue.insert(it, cmd);
}

bool InputCommandInbox::IsInWindow(uint32_t sequence) const noexcept
{
    const uint32_t oldest = (m_lastProcessedSequence >= kInboxCapacity)
        ? m_lastProcessedSequence - static_cast<uint32_t>(kInboxCapacity)
        : 0u;
    return sequence > oldest;
}

} // namespace SagaEngine::Input