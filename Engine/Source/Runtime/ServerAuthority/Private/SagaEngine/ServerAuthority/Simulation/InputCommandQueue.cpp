/// @file InputCommandQueue.cpp
/// @brief InputCommandQueue and InputCommandRouter implementations.

#include "SagaServer/Simulation/InputCommandQueue.h"
#include "SagaEngine/Core/Log/Log.h"

#include <algorithm>
#include <chrono>
#include <utility>

namespace SagaEngine::Server::Simulation
{

static constexpr const char* kTag = "InputQueue";

namespace
{

[[nodiscard]] std::uint64_t NowUnixMs() noexcept
{
    using clock = std::chrono::system_clock;
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            clock::now().time_since_epoch()).count());
}

} // anonymous namespace

// ─── InputCommandQueue ────────────────────────────────────────────────────────

InputCommandQueue::InputCommandQueue(ClientId clientId, InputQueueConfig config)
    : m_ClientId(clientId)
    , m_Config(config)
{
    m_Ring.reserve(m_Config.capacity);
    m_SeenSequences.reserve(m_Config.duplicateWindow);
}

bool InputCommandQueue::IsDuplicateLocked(std::uint64_t sequence) const noexcept
{
    return m_SeenSequences.find(sequence) != m_SeenSequences.end();
}

void InputCommandQueue::RecordSequenceLocked(std::uint64_t sequence)
{
    m_SeenSequences[sequence] = 1;

    if (m_SeenSequences.size() > m_Config.duplicateWindow)
    {
        const std::uint64_t floor =
            sequence > m_Config.duplicateWindow
            ? sequence - m_Config.duplicateWindow
            : 0;

        for (auto it = m_SeenSequences.begin(); it != m_SeenSequences.end(); )
        {
            if (it->first < floor)
                it = m_SeenSequences.erase(it);
            else
                ++it;
        }
    }
}

bool InputCommandQueue::IsInWindowLocked(std::uint64_t commandTick,
                                          std::uint64_t serverTick) const noexcept
{
    if (commandTick > serverTick)
    {
        const std::uint64_t lead = commandTick - serverTick;
        return lead <= m_Config.maxJitterTicks;
    }

    const std::uint64_t lag = serverTick - commandTick;
    return lag <= m_Config.maxLagTicks;
}

bool InputCommandQueue::Enqueue(const InputCommand& cmd)
{
    std::lock_guard<std::mutex> lock(m_Mutex);

    if (IsDuplicateLocked(cmd.sequence))
    {
        ++m_Stats.duplicates;
        return false;
    }

    if (cmd.sequence <= m_LastDequeuedSeq && m_Config.rejectOutOfOrder)
    {
        ++m_Stats.dropped;
        return false;
    }

    if (!IsInWindowLocked(cmd.clientTick, cmd.serverRecvTick))
    {
        ++m_Stats.outOfWindow;
        LOG_WARN(kTag,
            "Client %llu: command seq %llu out of window (client %llu / server %llu)",
            static_cast<unsigned long long>(m_ClientId),
            static_cast<unsigned long long>(cmd.sequence),
            static_cast<unsigned long long>(cmd.clientTick),
            static_cast<unsigned long long>(cmd.serverRecvTick));
        return false;
    }

    if (m_Ring.size() >= m_Config.capacity)
    {
        ++m_Stats.overruns;
        m_Ring.erase(m_Ring.begin());
    }

    const auto insertPos = std::upper_bound(
        m_Ring.begin(), m_Ring.end(), cmd,
        [](const InputCommand& a, const InputCommand& b)
        {
            return a.sequence < b.sequence;
        });

    m_Ring.insert(insertPos, cmd);

    RecordSequenceLocked(cmd.sequence);
    ++m_Stats.enqueued;
    m_Stats.lastEnqueueUnixMs = NowUnixMs();
    return true;
}

std::optional<InputCommand> InputCommandQueue::Dequeue(std::uint64_t serverTick)
{
    std::lock_guard<std::mutex> lock(m_Mutex);

    if (m_Ring.empty())
        return std::nullopt;

    const auto& front = m_Ring.front();
    if (front.clientTick > serverTick + m_Config.maxJitterTicks)
        return std::nullopt;

    InputCommand cmd = front;
    m_Ring.erase(m_Ring.begin());

    m_LastDequeuedSeq        = cmd.sequence;
    ++m_Stats.dequeued;
    m_Stats.lastDequeueUnixMs = NowUnixMs();

    return cmd;
}

std::vector<InputCommand> InputCommandQueue::DrainForTick(std::uint64_t serverTick,
                                                           std::size_t   maxCount)
{
    std::vector<InputCommand> out;
    std::lock_guard<std::mutex> lock(m_Mutex);

    out.reserve(std::min(maxCount, m_Ring.size()));

    while (!m_Ring.empty() && out.size() < maxCount)
    {
        const auto& front = m_Ring.front();
        if (front.clientTick > serverTick + m_Config.maxJitterTicks)
            break;

        out.push_back(front);
        m_LastDequeuedSeq = front.sequence;
        m_Ring.erase(m_Ring.begin());
        ++m_Stats.dequeued;
    }

    if (!out.empty())
        m_Stats.lastDequeueUnixMs = NowUnixMs();

    return out;
}

void InputCommandQueue::Flush()
{
    std::lock_guard<std::mutex> lock(m_Mutex);
    m_Ring.clear();
    m_SeenSequences.clear();
}

std::size_t InputCommandQueue::GetPending() const
{
    std::lock_guard<std::mutex> lock(m_Mutex);
    return m_Ring.size();
}

InputQueueStats InputCommandQueue::GetStatistics() const
{
    std::lock_guard<std::mutex> lock(m_Mutex);
    return m_Stats;
}

std::uint64_t InputCommandQueue::GetLastDequeuedSequence() const
{
    std::lock_guard<std::mutex> lock(m_Mutex);
    return m_LastDequeuedSeq;
}

// ─── InputCommandRouter ───────────────────────────────────────────────────────

InputCommandRouter::InputCommandRouter(InputQueueConfig defaults)
    : m_Defaults(defaults)
{
}

bool InputCommandRouter::AddClient(ClientId clientId)
{
    std::lock_guard<std::mutex> lock(m_Mutex);

    if (m_Queues.find(clientId) != m_Queues.end())
        return false;

    m_Queues.emplace(
        clientId,
        std::make_unique<InputCommandQueue>(clientId, m_Defaults));

    LOG_INFO(kTag, "Router: added client %llu",
             static_cast<unsigned long long>(clientId));
    return true;
}

void InputCommandRouter::RemoveClient(ClientId clientId)
{
    std::lock_guard<std::mutex> lock(m_Mutex);
    m_Queues.erase(clientId);
}

bool InputCommandRouter::HasClient(ClientId clientId) const
{
    std::lock_guard<std::mutex> lock(m_Mutex);
    return m_Queues.find(clientId) != m_Queues.end();
}

bool InputCommandRouter::Enqueue(ClientId clientId, const InputCommand& cmd)
{
    std::lock_guard<std::mutex> lock(m_Mutex);

    auto it = m_Queues.find(clientId);
    if (it == m_Queues.end())
    {
        LOG_WARN(kTag, "Enqueue: unknown client %llu",
                 static_cast<unsigned long long>(clientId));
        return false;
    }

    return it->second->Enqueue(cmd);
}

std::optional<InputCommand> InputCommandRouter::Dequeue(ClientId     clientId,
                                                         std::uint64_t serverTick)
{
    std::lock_guard<std::mutex> lock(m_Mutex);

    auto it = m_Queues.find(clientId);
    if (it == m_Queues.end())
        return std::nullopt;

    return it->second->Dequeue(serverTick);
}

std::size_t InputCommandRouter::DrainAll(
    std::uint64_t                                                  serverTick,
    std::size_t                                                    maxPerClient,
    std::unordered_map<ClientId, std::vector<InputCommand>>&       outByClient)
{
    std::lock_guard<std::mutex> lock(m_Mutex);

    std::size_t total = 0;

    for (const auto& [clientId, queuePtr] : m_Queues)
    {
        auto commands = queuePtr->DrainForTick(serverTick, maxPerClient);
        if (!commands.empty())
        {
            total += commands.size();
            outByClient[clientId] = std::move(commands);
        }
    }

    return total;
}

std::vector<ClientId> InputCommandRouter::ListClients() const
{
    std::lock_guard<std::mutex> lock(m_Mutex);

    std::vector<ClientId> out;
    out.reserve(m_Queues.size());
    for (const auto& [clientId, _] : m_Queues)
        out.push_back(clientId);
    return out;
}

InputQueueStats InputCommandRouter::GetStatistics(ClientId clientId) const
{
    std::lock_guard<std::mutex> lock(m_Mutex);

    auto it = m_Queues.find(clientId);
    if (it == m_Queues.end())
        return {};

    return it->second->GetStatistics();
}

} // namespace SagaEngine::Server::Simulation
