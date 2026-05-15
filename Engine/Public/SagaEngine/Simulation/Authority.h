/// @file Authority.h
/// @brief Server-side simulation orchestrator: owns WorldState, runs systems per tick.
///
/// Authority is the authoritative simulation loop. Each server tick it:
///   1. Collects validated InputCommands from ServerInputProcessor.
///   2. Feeds them to each registered ISimulationSystem in registration order.
///   3. Each system reads/writes WorldState directly.
///   4. After all systems run, Authority records the tick hash via Deterministic.
///
/// System registration order determines execution order — register movement
/// before collision, collision before ability resolution, and so on.
///
/// Thread safety:
///   Authority is NOT thread-safe. All calls must come from the simulation thread.
///   The WorldState it exposes for replication must be snapshotted by the caller
///   before handing to another thread (double-buffer pattern).

#pragma once

#include "SagaEngine/Simulation/WorldState.h"
#include "SagaEngine/Simulation/Deterministic.h"
#include "SagaEngine/Input/Networking/ServerInputProcessor.h"

#include <memory>
#include <string_view>
#include <vector>

namespace SagaEngine::Simulation {

/// Interface for one simulation system.
///
/// Each system encapsulates a single responsibility (movement, combat, spawning).
/// Systems are stateless with respect to the simulation — all mutable state lives
/// in WorldState. A system may hold read-only config or caches, but never
/// authoritative entity data.
class ISimulationSystem
{
public:
    virtual ~ISimulationSystem() = default;

    /// Human-readable name for logging and profiling.
    [[nodiscard]] virtual const char* Name() const noexcept = 0;

    /// Execute this system for one simulation tick.
    ///
    /// @param state    Mutable world state — read and write freely.
    /// @param entries  Per-client input for this tick (server-validated).
    /// @param tick     Current authoritative tick number.
    /// @param dt       Fixed simulation delta time in seconds (1 / tickRate).
    virtual void Update(
        WorldState&                                             state,
        std::span<const Input::ClientTickEntry> entries,
        uint64_t                                                tick,
        double                                                  dt) = 0;
};

/// Per-tick result exposed to the server loop after Authority::Tick() completes.
struct AuthorityTickResult
{
    uint64_t tick{0};       ///< Tick that was just simulated.
    uint64_t stateHash{0};  ///< Deterministic::Record() output — embed in replication.
    bool     diverged{false};///< True if any client reported a mismatching hash.
};

class AuthorityManager
{
public:
    void SetServerMode(bool enabled) noexcept { m_serverMode = enabled; }
    void SetClientId(uint32_t clientId) noexcept { m_clientId = clientId; }

    [[nodiscard]] bool CanModify(uint32_t /*entityId*/) const noexcept
    {
        return m_serverMode;
    }

    [[nodiscard]] bool IsServerMode() const noexcept { return m_serverMode; }
    [[nodiscard]] uint32_t GetClientId() const noexcept { return m_clientId; }

private:
    bool     m_serverMode{false};
    uint32_t m_clientId{0};
};

/// Authoritative simulation orchestrator.
class Authority
{
public:
    Authority() = default;
    ~Authority() = default;

    Authority(const Authority&) = delete;
    Authority& operator=(const Authority&) = delete;

    // ─── System registration ───────────────────────────────────────────────────

    /// Register a simulation system. Systems execute in registration order.
    ///
    /// All systems must be registered before the first Tick() call.
    void RegisterSystem(std::unique_ptr<ISimulationSystem> system);

    // ─── State access ──────────────────────────────────────────────────────────

    /// Return a mutable reference to the current authoritative world state.
    ///
    /// Use for initial entity spawning and server-side administrative mutations.
    /// During normal play, prefer the update path through ISimulationSystem.
    [[nodiscard]] WorldState& GetWorldState() noexcept { return m_world; }

    [[nodiscard]] const WorldState& GetWorldState() const noexcept { return m_world; }

    /// Return the determinism tracker for hash lookups by the replication layer.
    [[nodiscard]] const Deterministic& GetDeterministic() const noexcept { return m_deterministic; }

    // ─── Tick ──────────────────────────────────────────────────────────────────

    /// Advance the simulation by one tick.
    ///
    /// @param tick     The authoritative tick number being processed.
    /// @param dt       Fixed delta time in seconds from SimulationTick.
    /// @param entries  Validated per-client input from ServerInputProcessor::ProcessTick().
    /// @returns        Tick result containing the new state hash and divergence flag.
    AuthorityTickResult Tick(
        uint64_t                                                     tick,
        double                                                       dt,
        std::span<const SagaEngine::Input::ClientTickEntry> entries);

    // ─── Diagnostics ───────────────────────────────────────────────────────────

    [[nodiscard]] uint32_t SystemCount() const noexcept
    {
        return static_cast<uint32_t>(m_systems.size());
    }

private:
    WorldState                              m_world;        ///< Single authoritative state.
    Deterministic                           m_deterministic;///< Rolling hash history.
    std::vector<std::unique_ptr<ISimulationSystem>> m_systems; ///< Ordered system pipeline.
};

} // namespace SagaEngine::Simulation
