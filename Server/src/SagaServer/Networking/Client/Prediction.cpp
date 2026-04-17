/// @file Prediction.cpp
/// @brief Server-side client prediction bookkeeping implementation.
///
/// Layer  : SagaServer / Networking / Client
/// Purpose: Production implementation of the server-side prediction tracker
///          and bookkeeper with sequence validation, circular history buffer,
///          and thread-safe multi-client management.

#include "SagaServer/Networking/Client/Prediction.h"
#include "SagaEngine/Core/Log/Log.h"

namespace SagaServer::Networking {

static constexpr const char* kTag = "ClientPrediction";

// ─── ClientPredictionTracker::ProcessCommand ────────────────────────────

/// Validate and process a single input command.
///
/// Sequence validation:
///   - If sequence == highestProcessed → duplicate (retransmit)
///   - If sequence <= highestProcessed - kHistorySize → too old
///   - If sequence > highestProcessed + 1 → too new (gap detected)
///   - Otherwise → accepted, advance highestProcessed
///
/// @param command     Client's input command.
/// @param serverTick  Current server tick.
/// @param handler     Callback to apply command to world state.
/// @return CommandResult with processing status.
CommandResult ClientPredictionTracker::ProcessCommand(
    const ClientInputCommand& command, ServerTick serverTick,
    CommandApplyFn handler) noexcept
{
    CommandResult result;
    result.sequence = command.sequence;
    result.serverTick = serverTick;
    result.status = CommandStatus::InternalError;
    result.resultFlags = 0;

    // Validate entity ownership.
    if (m_controlledEntity == 0 || command.entityId != m_controlledEntity)
    {
        result.status = CommandStatus::InvalidEntity;
        LOG_WARN(kTag, "Client %llu: command seq %u targets entity %u (expected %u)",
                 static_cast<unsigned long long>(m_clientId),
                 command.sequence, command.entityId, m_controlledEntity);
        return result;
    }

    // Validate sequence number.
    if (m_highestProcessed == kInvalidSequence)
    {
        // First command: accept if sequence is 1.
        if (command.sequence != 1)
        {
            result.status = CommandStatus::TooNew;
            LOG_WARN(kTag, "Client %llu: first command seq %u (expected 1)",
                     static_cast<unsigned long long>(m_clientId), command.sequence);
            return result;
        }
    }
    else if (command.sequence == m_highestProcessed)
    {
        result.status = CommandStatus::Duplicate;
        // Return the existing result from history (if available).
        const CommandResult* existing = GetResult(command.sequence);
        if (existing)
            return *existing;
        return result;
    }
    else if (command.sequence < m_highestProcessed)
    {
        // Check if it's within the history window.
        if (m_highestProcessed - command.sequence >= kHistorySize)
        {
            result.status = CommandStatus::TooOld;
            return result;
        }
        // Stale but in-range: return duplicate.
        result.status = CommandStatus::Duplicate;
        const CommandResult* existing = GetResult(command.sequence);
        if (existing)
            return *existing;
        return result;
    }
    else if (command.sequence > m_highestProcessed + 1)
    {
        result.status = CommandStatus::TooNew;
        LOG_WARN(kTag, "Client %llu: command seq %u has gap (expected %u)",
                 static_cast<unsigned long long>(m_clientId),
                 command.sequence, m_highestProcessed + 1);
        return result;
    }

    // Command is valid and in sequence: apply it.
    if (handler)
    {
        handler(command);
    }

    m_highestProcessed = command.sequence;

    result.status = CommandStatus::Accepted;
    result.serverTick = serverTick;

    // Store in circular history buffer.
    const uint32_t idx = m_historyIndex % kHistorySize;
    m_history[idx] = result;
    m_historyIndex++;

    return result;
}

// ─── ClientPredictionTracker::GetResult ─────────────────────────────────

/// Look up a command result from the circular history buffer.
///
/// The buffer stores the last kHistorySize results.  We search backwards
/// from the most recent entry to find a matching sequence.
///
/// @param sequence  Command sequence to look up.
/// @return Pointer to the result in the buffer, or nullptr if not found.
const CommandResult* ClientPredictionTracker::GetResult(InputSequence sequence) const noexcept
{
    if (sequence == kInvalidSequence || sequence > m_highestProcessed)
        return nullptr;

    // Search the circular buffer for the matching sequence.
    // The buffer contains results for sequences: [highestProcessed - kHistorySize + 1 .. highestProcessed]
    const InputSequence oldestInHistory = (m_highestProcessed >= kHistorySize)
                                              ? m_highestProcessed - kHistorySize + 1
                                              : 1;

    if (sequence < oldestInHistory)
        return nullptr;  // Too old; evicted from history.

    // Calculate the buffer index for this sequence.
    // The buffer is written sequentially; index wraps at kHistorySize.
    const uint32_t bufferIndex = (sequence - 1) % kHistorySize;
    const CommandResult& entry = m_history[bufferIndex];

    if (entry.sequence == sequence)
        return &entry;

    return nullptr;  // Slot not yet written or mismatched.
}

// ─── ClientPredictionTracker::Reset ─────────────────────────────────────

void ClientPredictionTracker::Reset() noexcept
{
    m_highestProcessed = kInvalidSequence;
    m_historyIndex = 0;
    for (auto& entry : m_history)
        entry = CommandResult{};
}

// ─── ClientPredictionBookkeeper::CreateTracker ──────────────────────────

ClientPredictionTracker* ClientPredictionBookkeeper::CreateTracker(uint64_t clientId) noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);

    // If a tracker already exists for this client, return it.
    auto it = m_trackers.find(clientId);
    if (it != m_trackers.end())
        return it->second.get();

    auto tracker = std::make_unique<ClientPredictionTracker>(clientId);
    ClientPredictionTracker* rawPtr = tracker.get();
    m_trackers.emplace(clientId, std::move(tracker));

    LOG_DEBUG(kTag, "Created prediction tracker for client %llu",
              static_cast<unsigned long long>(clientId));

    return rawPtr;
}

// ─── ClientPredictionBookkeeper::RemoveTracker ──────────────────────────

void ClientPredictionBookkeeper::RemoveTracker(uint64_t clientId) noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_trackers.erase(clientId);

    LOG_DEBUG(kTag, "Removed prediction tracker for client %llu",
              static_cast<unsigned long long>(clientId));
}

// ─── ClientPredictionBookkeeper::ProcessCommand ─────────────────────────

CommandResult ClientPredictionBookkeeper::ProcessCommand(
    uint64_t clientId, const ClientInputCommand& command, ServerTick serverTick,
    ClientPredictionTracker::CommandApplyFn handler) noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_trackers.find(clientId);
    if (it == m_trackers.end())
    {
        LOG_WARN(kTag, "Client %llu: no prediction tracker; command seq %u rejected",
                 static_cast<unsigned long long>(clientId), command.sequence);

        CommandResult result;
        result.sequence = command.sequence;
        result.serverTick = serverTick;
        result.status = CommandStatus::InternalError;
        return result;
    }

    return it->second->ProcessCommand(command, serverTick, std::move(handler));
}

// ─── ClientPredictionBookkeeper::GetResults ─────────────────────────────

bool ClientPredictionBookkeeper::GetResults(
    uint64_t clientId, InputSequence sinceSequence,
    std::vector<CommandResult>& outResults) const noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_trackers.find(clientId);
    if (it == m_trackers.end())
        return false;

    const auto* tracker = it->second.get();

    // Walk through the history buffer and collect results > sinceSequence.
    const InputSequence highest = tracker->HighestProcessed();
    if (highest == kInvalidSequence)
        return true;  // No commands processed yet.

    const InputSequence oldestInHistory = (highest >= ClientPredictionTracker::kHistorySize)
                                              ? highest - ClientPredictionTracker::kHistorySize + 1
                                              : 1;

    const InputSequence startSeq = (sinceSequence >= oldestInHistory)
                                       ? sinceSequence + 1
                                       : oldestInHistory;

    for (InputSequence seq = startSeq; seq <= highest; ++seq)
    {
        const CommandResult* result = tracker->GetResult(seq);
        if (result)
            outResults.push_back(*result);
    }

    return true;
}

// ─── ClientPredictionBookkeeper::GetHighestProcessed ────────────────────

InputSequence ClientPredictionBookkeeper::GetHighestProcessed(uint64_t clientId) const noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_trackers.find(clientId);
    if (it == m_trackers.end())
        return kInvalidSequence;

    return it->second->HighestProcessed();
}

// ─── ClientPredictionBookkeeper::ResetAll ───────────────────────────────

void ClientPredictionBookkeeper::ResetAll() noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);

    for (auto& [_, tracker] : m_trackers)
        tracker->Reset();

    LOG_INFO(kTag, "Reset all prediction trackers (%zu trackers)", m_trackers.size());
}

// ─── ClientPredictionBookkeeper::TrackerCount ───────────────────────────

uint32_t ClientPredictionBookkeeper::TrackerCount() const noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return static_cast<uint32_t>(m_trackers.size());
}

} // namespace SagaServer::Networking
