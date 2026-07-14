/// @file ServerInputProcessor.cpp
/// @brief Validates and dispatches input commands within a server simulation tick.

#include "SagaEngine/Input/Networking/ServerInputProcessor.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <utility>

namespace SagaEngine::Input
{

using SagaEngine::Networking::ConnectionId;
using InputCommand = SagaEngine::Input::InputCommand;

namespace
{
    /// Clamp a floating-point config value to the fixed-point command domain.
    [[nodiscard]] constexpr int32_t ToFixedLimit(float value) noexcept
    {
        return FixedFromFloat(value);
    }
} // namespace

// ─── Lifecycle ────────────────────────────────────────────────────────────────

ServerInputProcessor::ServerInputProcessor(InputValidationConfig config)
    : m_config(config)
{
}

void ServerInputProcessor::AddClient(uint32_t clientId)
{
    if (m_inboxes.contains(clientId))
    {
        return;
    }

    m_inboxes.emplace(
        clientId,
        std::make_unique<InputCommandInbox>(static_cast<ConnectionId>(clientId)));
}

void ServerInputProcessor::RemoveClient(uint32_t clientId)
{
    m_inboxes.erase(clientId);
}

InputCommandInbox* ServerInputProcessor::GetInbox(uint32_t clientId)
{
    auto it = m_inboxes.find(clientId);
    return it != m_inboxes.end() ? it->second.get() : nullptr;
}

// ─── Per-tick processing ─────────────────────────────────────────────────────

ServerInputProcessor::TickResult
ServerInputProcessor::ProcessTick(uint32_t serverTick)
{
    TickResult result;
    result.entries.reserve(m_inboxes.size());
    result.acks.reserve(m_inboxes.size());

    for (auto& [clientId, inbox] : m_inboxes)
    {
        ClientTickEntry entry{};
        entry.clientId = clientId;

        const auto tickResult = inbox->ConsumeForTick(serverTick);

        if (!tickResult.command.has_value())
        {
            entry.hasCommand = false;
            entry.wasLate = true;
            entry.wasInvalid = false;
            result.entries.push_back(entry);
            continue;
        }

        entry.hasCommand = true;
        entry.wasLate = tickResult.isLate;

        const InputCommand& raw = tickResult.command.value();
        const bool valid = ValidateCommand(raw, serverTick);
        entry.wasInvalid = !valid;
        entry.command = Sanitize(raw);

        result.entries.push_back(entry);

        // Ack only real consumed commands. Fallbacks are not new input.
        if (!tickResult.usedFallback)
        {
            result.acks.push_back(AckEntry{
                .clientId = clientId,
                .sequence  = inbox->GetLastProcessedSequence()
            });
        }
    }

    return result;
}

// ─── Validation ──────────────────────────────────────────────────────────────

bool ServerInputProcessor::ValidateCommand(
    const InputCommand& cmd,
    uint32_t serverTick) const noexcept
{
    if (cmd.version != InputCommand::kCurrentVersion)
    {
        return false;
    }

    const int32_t moveLimit = ToFixedLimit(m_config.maxMoveValue);
    const int32_t lookLimit = ToFixedLimit(m_config.maxLookDelta);

    if (std::abs(cmd.moveX) > moveLimit) return false;
    if (std::abs(cmd.moveY) > moveLimit) return false;
    if (std::abs(cmd.lookX) > lookLimit) return false;
    if (std::abs(cmd.lookY) > lookLimit) return false;

    if ((cmd.buttons & m_config.forbiddenButtonMask) != 0u)
    {
        return false;
    }

    if (m_config.enforceTickWindow && serverTick > 0u)
    {
        if (cmd.simulationTick > serverTick + m_config.maxTickAheadTolerance)
        {
            return false;
        }

        if (serverTick > m_config.maxTickBehindTolerance
            && cmd.simulationTick < serverTick - m_config.maxTickBehindTolerance)
        {
            return false;
        }
    }

    return true;
}

// ─── Sanitization ────────────────────────────────────────────────────────────

InputCommand ServerInputProcessor::Sanitize(
    const InputCommand& cmd) const noexcept
{
    InputCommand out = cmd;

    auto Clamp = [](int32_t value, int32_t limit) noexcept -> int32_t
    {
        if (value > limit) return limit;
        if (value < -limit) return -limit;
        return value;
    };

    const int32_t moveLimit = ToFixedLimit(m_config.maxMoveValue);
    const int32_t lookLimit = ToFixedLimit(m_config.maxLookDelta);

    out.moveX = Clamp(cmd.moveX, moveLimit);
    out.moveY = Clamp(cmd.moveY, moveLimit);
    out.lookX = Clamp(cmd.lookX, lookLimit);
    out.lookY = Clamp(cmd.lookY, lookLimit);
    out.buttons &= ~m_config.forbiddenButtonMask;
    out.version = InputCommand::kCurrentVersion;

    return out;
}

} // namespace SagaEngine::Input