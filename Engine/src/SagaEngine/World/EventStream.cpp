/// @file EventStream.cpp
/// @brief Append-only event log implementation.

#include "SagaEngine/World\EventStream.h"
#include <algorithm>

namespace SagaEngine::World {

void EventStream::Append(WorldEvent evt) noexcept
{
    m_lastSequence++;
    evt.sequence = m_lastSequence;
    m_events.push_back(std::move(evt));
}

void EventStream::AppendBatch(const std::vector<WorldEvent>& events) noexcept
{
    m_events.reserve(m_events.size() + events.size());
    for (const auto& evt : events)
    {
        m_lastSequence++;
        WorldEvent e = evt;
        e.sequence = m_lastSequence;
        m_events.push_back(std::move(e));
    }
}

void EventStream::TakeSnapshot(WorldSnapshot snap) noexcept
{
    snap.baseSequence = m_lastSequence;
    m_snapshots.push_back(std::move(snap));
}

const WorldSnapshot* EventStream::LatestSnapshot() const noexcept
{
    if (m_snapshots.empty())
        return nullptr;
    return &m_snapshots.back();
}

void EventStream::ReplayFromStart(EventHandlerFn handler) const noexcept
{
    for (const auto& evt : m_events)
        handler(evt);
}

void EventStream::ReplayFromSnapshot(EventHandlerFn handler) const noexcept
{
    const auto* snap = LatestSnapshot();
    if (!snap)
    {
        ReplayFromStart(std::move(handler));
        return;
    }

    // Replay events after the snapshot's base sequence.
    ReplayRange(snap->baseSequence + 1, m_lastSequence, std::move(handler));
}

void EventStream::ReplayRange(uint64_t fromSeq, uint64_t toSeq,
                                EventHandlerFn handler) const noexcept
{
    for (const auto& evt : m_events)
    {
        if (evt.sequence < fromSeq)
            continue;
        if (evt.sequence > toSeq)
            break;
        handler(evt);
    }
}

const WorldEvent* EventStream::GetEvent(uint64_t seq) const noexcept
{
    if (seq == 0 || seq > m_lastSequence)
        return nullptr;

    // Binary search would be better for large streams, but linear scan
    // is fine for < 1M events.  Production should use an index.
    for (const auto& evt : m_events)
    {
        if (evt.sequence == seq)
            return &evt;
    }
    return nullptr;
}

std::vector<WorldEvent> EventStream::GetEventsAfter(uint64_t lastSeenSeq) const noexcept
{
    std::vector<WorldEvent> out;
    out.reserve(m_events.size()); // upper bound

    bool found = false;
    for (const auto& evt : m_events)
    {
        if (!found)
        {
            if (evt.sequence == lastSeenSeq)
                found = true;
            continue;
        }
        out.push_back(evt);
    }

    return out;
}

} // namespace SagaEngine::World
