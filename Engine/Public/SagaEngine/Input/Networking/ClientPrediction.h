#pragma once

/// @file ClientPrediction.h
/// @brief Client-side prediction: apply local commands before server confirms.
///
/// Layer  : Input / Prediction
/// Purpose: In an authoritative-server MMO, the client simulates its own
///          character locally using its own input commands, without waiting
///          for the round-trip to the server. When the server sends back
///          the authoritative state, the client reconciles.
///
/// Prediction loop (per client tick):
///   1. InputManager::Update() → GameplayInputFrame
///   2. Build InputCommand from frame → push to InputCommandBuffer
///   3. ClientPrediction::ApplyLocalCommand(cmd) → simulate locally
///   4. ClientPrediction::SaveSnapshot(tick, state) → store for reconcile
///   5. Network sends command to server
///
/// Reconciliation loop (when server state arrives):
///   1. ClientPrediction::Reconcile(serverTick, serverState)
///   2. If local state at serverTick differs from serverState:
///      a. Rollback simulation to serverTick
///      b. Re-apply all unacked commands from serverTick onward
///      c. Re-render from corrected state
///
/// Design notes:
///   - WorldState is template-parameterized. The prediction system does not
///     know what a "world state" contains — only that it can be compared and
///     applied. The game provides an ApplyFn and a CompareFn.
///   - Snapshot ring buffer size = kSnapshotDepth ticks. Commands older
///     than this cannot be reconciled (server is too far behind — disconnect).
///   - This class is NOT thread-safe. Call from simulation thread only.

#include "SagaEngine/Input/Commands/InputCommand.h"
#include <array>
#include <concepts>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <vector>

namespace SagaEngine::Input
{

/// Maximum number of ticks we keep snapshots for reconciliation.
/// At 64 ticks/sec: 256 ticks = 4 seconds of history.
inline constexpr size_t kSnapshotDepth = 256;

// Concepts for WorldState

template<typename T>
concept WorldStateConcept = requires(T a, const T& b)
{
    { a = b } -> std::same_as<T&>;   // copyable
    { T{} };                          // default constructible
};

// Snapshot

template<WorldStateConcept WorldState>
struct PredictionSnapshot
{
    uint32_t   tick     = 0;
    bool       valid    = false;
    WorldState state;
    InputCommand command;  ///< Command that produced this state
};

// Callbacks

template<WorldStateConcept WorldState>
using SimulateOneFn = std::function<
    WorldState(const WorldState& currentState, const InputCommand& cmd)
>;

template<WorldStateConcept WorldState>
using StatesEqualFn = std::function<
    bool(const WorldState& predicted, const WorldState& authoritative)
>;

// Client Prediction

template<WorldStateConcept WorldState>
class ClientPrediction
{
public:
    struct Config
    {
        SimulateOneFn<WorldState> simulate;       ///< Required: one-tick sim function
        StatesEqualFn<WorldState> statesEqual;    ///< Required: divergence check
        float                     reconcileThresholdSq = 0.01f; ///< Spatial tolerance²
    };

    explicit ClientPrediction(Config config)
        : m_config(std::move(config))
    {}

    // Prediction

    /// Apply a local command to the current predicted state.
    /// Stores a snapshot for later reconciliation.
    /// Call this after building the InputCommand, before sending to network.
    void ApplyLocalCommand(const InputCommand& cmd, const WorldState& stateBefore)
    {
        const WorldState stateAfter = m_config.simulate(stateBefore, cmd);

        auto& slot = SnapshotSlot(cmd.simulationTick);
        slot.tick    = cmd.simulationTick;
        slot.valid   = true;
        slot.state   = stateAfter;
        slot.command = cmd;

        m_latestPredictedTick = cmd.simulationTick;
        m_currentPredictedState = stateAfter;
    }

    // Reconciliation

    struct ReconcileResult
    {
        bool      corrected = false;          ///< true if a divergence was found
        uint32_t  divergedAtTick = 0;
        WorldState correctedState;
    };

    /// Called when server sends authoritative state for a confirmed tick.
    /// @param serverTick       The tick the server has simulated through
    /// @param authState        Server-authoritative world state at serverTick
    /// @param unackedCommands  Commands after serverTick still to re-apply
    ReconcileResult Reconcile(
        uint32_t                      serverTick,
        const WorldState&             authState,
        std::span<const InputCommand> unackedCommands)
    {
        ReconcileResult result;
        result.correctedState = authState;

        // Check divergence
        const auto& snapshot = SnapshotSlot(serverTick);
        if (snapshot.valid && snapshot.tick == serverTick)
        {
            if (m_config.statesEqual(snapshot.state, authState))
            {
                // No divergence — prediction was correct
                return result;
            }
        }

        // Diverged: replay from server tick forward
        result.corrected      = true;
        result.divergedAtTick = serverTick;

        WorldState state = authState;
        for (const auto& cmd : unackedCommands)
        {
            if (cmd.simulationTick <= serverTick) continue;
            state = m_config.simulate(state, cmd);

            auto& slot = SnapshotSlot(cmd.simulationTick);
            slot.tick    = cmd.simulationTick;
            slot.valid   = true;
            slot.state   = state;
            slot.command = cmd;
        }

        m_currentPredictedState = state;
        result.correctedState   = state;
        return result;
    }

    // State access

    [[nodiscard]] const WorldState& GetPredictedState() const noexcept
    {
        return m_currentPredictedState;
    }

    [[nodiscard]] uint32_t GetLatestPredictedTick() const noexcept
    {
        return m_latestPredictedTick;
    }

    [[nodiscard]] std::optional<WorldState> GetSnapshotAt(uint32_t tick) const
    {
        const auto& slot = SnapshotSlot(tick);
        if (slot.valid && slot.tick == tick) return slot.state;
        return std::nullopt;
    }

    void Reset()
    {
        for (auto& s : m_snapshots) s.valid = false;
        m_latestPredictedTick = 0;
    }

private:
    using SnapshotArray = std::array<PredictionSnapshot<WorldState>, kSnapshotDepth>;

    PredictionSnapshot<WorldState>& SnapshotSlot(uint32_t tick)
    {
        return m_snapshots[tick % kSnapshotDepth];
    }

    const PredictionSnapshot<WorldState>& SnapshotSlot(uint32_t tick) const
    {
        return m_snapshots[tick % kSnapshotDepth];
    }

    Config          m_config;
    SnapshotArray   m_snapshots{};
    WorldState      m_currentPredictedState{};
    uint32_t        m_latestPredictedTick = 0;
};

} // namespace SagaEngine::Input