#pragma once

/// @file ServerInputProcessor.h
/// @brief Validates and dispatches input commands within a server simulation tick.
///
/// Layer  : Server / Input
/// Purpose: Called once per simulation tick by the game server loop.
///          For each connected client, fetches the best available command
///          from its InputCommandInbox, validates it, and delivers a
///          clean, authoritative InputCommand to the simulation.
///
/// Responsibilities:
///   - Anti-cheat validation (value clamping, rate checks)
///   - Input delay / latency compensation window management
///   - Late command handling policy (use last, use zero, extrapolate)
///   - Building per-tick ack list for the network layer
///
/// Explicitly NOT responsible for:
///   - Parsing / deserializing packets (network layer does that)
///   - Storing raw commands (InputCommandInbox does that)
///   - Moving or simulating entities (simulation does that)
///   - Any client-side logic
///
/// Usage per tick:
///   ServerInputProcessor::TickResult results = processor.ProcessTick(serverTick);
///   for (auto& entry : results.entries)
///   {
///       if (entry.hasCommand)
///           simulation.ApplyInput(entry.clientId, entry.command);
///   }
///   network.SendAcks(results.acks);

#include "SagaEngine/Input/Networking/InputCommandInbox.h"
#include "SagaEngine/Input/Commands/InputCommand.h"
#include <memory>
#include <span>
#include <unordered_map>
#include <vector>
#include <cstdint>

namespace SagaEngine::Input
{

// Validation rules

struct InputValidationConfig
{
    /// Maximum allowed analog value (movement/look axes are clamped to this).
    float maxMoveValue = 1.f;
    float maxLookDelta = 32.f;   ///< In fixed-point world units per tick

    /// Buttons that are explicitly forbidden (e.g., dev-only actions).
    uint32_t forbiddenButtonMask = 0;

    /// If true, reject commands with a simulationTick more than this far
    /// ahead of or behind the current server tick.
    bool  enforceTickWindow      = true;
    uint32_t maxTickAheadTolerance  = 4;   ///< Client running slightly fast
    uint32_t maxTickBehindTolerance = 32;  ///< Up to ~500ms late at 64hz
};

// Per-client tick result

struct ClientTickEntry
{
    uint32_t clientId   = 0;
    bool     hasCommand = false;
    bool     wasLate    = false;    ///< No command arrived in time
    bool     wasInvalid = false;    ///< Command arrived but failed validation
    SagaEngine::Input::InputCommand command{};
};

struct AckEntry
{
    uint32_t clientId = 0;
    uint32_t sequence = 0;     ///< Last processed sequence for this client
};

// Processor

class ServerInputProcessor
{
public:
    explicit ServerInputProcessor(InputValidationConfig config = {});
    ~ServerInputProcessor() = default;

    ServerInputProcessor(const ServerInputProcessor&) = delete;
    ServerInputProcessor& operator=(const ServerInputProcessor&) = delete;

    // Client management

    /// Register a new client inbox. Called when a client connects.
    void AddClient(uint32_t clientId);

    /// Remove a client. Called on disconnect.
    void RemoveClient(uint32_t clientId);

    /// Get the inbox for a specific client (for the network layer to push into).
    /// Returns nullptr if client is not registered.
    [[nodiscard]] InputCommandInbox* GetInbox(uint32_t clientId);

    // Per-tick processing

    struct TickResult
    {
        std::vector<ClientTickEntry> entries;  ///< One per registered client
        std::vector<AckEntry>        acks;     ///< To be sent back via network
    };

    /// Process all client inboxes for the given server tick.
    /// Returns validated commands and ack data.
    [[nodiscard]] TickResult ProcessTick(uint32_t serverTick);

    // Statistics

    [[nodiscard]] size_t GetConnectedClientCount() const noexcept
    {
        return m_inboxes.size();
    }

private:
    [[nodiscard]] bool ValidateCommand(
        const SagaEngine::Input::InputCommand& cmd,
        uint32_t                               serverTick
    ) const noexcept;

    SagaEngine::Input::InputCommand Sanitize(
        const SagaEngine::Input::InputCommand& cmd
    ) const noexcept;

    InputValidationConfig                                       m_config;
    std::unordered_map<uint32_t, std::unique_ptr<InputCommandInbox>> m_inboxes;
};

} // namespace SagaEngine::Input