/// @file ScenarioManager.h
/// @brief Manages the active scenario lifecycle and hot-swap between scenarios.
///
/// Layer  : Sandbox / Core
/// Purpose: SandboxHost delegates all scenario routing to ScenarioManager.
///          It is the ONLY place where IScenario::OnInit/OnUpdate/OnShutdown
///          are called. No other code may call these methods directly.
///
/// Hot-swap model:
///   ScenarioManager::RequestSwitch(id) is safe to call at any time.
///   The actual switch happens at the start of the next Update() call,
///   after the current tick completes (deferred to avoid re-entrant teardown).
///
/// Scenario registry:
///   Scenarios self-register via SAGA_SANDBOX_REGISTER_SCENARIO macro
///   (see ScenarioRegistry.h). ScenarioManager queries the registry at init.
///   No hard-coded scenario list exists in this class.

#pragma once

#include "IScenario.h"
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace SagaSandbox
{

// ─── Scenario Factory ─────────────────────────────────────────────────────────

/// Factory function that creates a new scenario instance.
using ScenarioFactory = std::function<std::unique_ptr<IScenario>()>;

// ─── ScenarioManager ──────────────────────────────────────────────────────────

/// Owns the active scenario and drives its lifecycle.
class ScenarioManager
{
public:
    ScenarioManager()  = default;
    ~ScenarioManager();

    ScenarioManager(const ScenarioManager&)            = delete;
    ScenarioManager& operator=(const ScenarioManager&) = delete;

    // ── Registration ──────────────────────────────────────────────────────────

    /// Register a factory under a unique scenario ID.
    /// Called at startup by ScenarioRegistry before any scenario runs.
    void RegisterScenario(std::string_view id, ScenarioFactory factory);

    // ── Lifecycle ─────────────────────────────────────────────────────────────

    /// Initialize: loads all registered scenarios and optionally activates one.
    /// @param initialScenarioId  Empty string = no auto-start (show picker only).
    [[nodiscard]] bool Init(std::string_view initialScenarioId = "");

    /// Drive the active scenario. Call once per application tick.
    void Update(float deltaTime, uint64_t tick);

    /// Render active scenario's debug UI. Call inside an ImGui frame.
    void RenderDebugUI();

    /// Tear down the active scenario and clear all state.
    void Shutdown();

    // ── Scenario Selection ────────────────────────────────────────────────────

    /// Request a switch to the scenario with the given ID.
    /// Switch is deferred to the next Update() boundary.
    void RequestSwitch(std::string_view id);

    /// Request shutdown of the active scenario without switching to another.
    void RequestStop();

    // ── Queries ───────────────────────────────────────────────────────────────

    [[nodiscard]] bool        HasActiveScenario()      const noexcept;
    [[nodiscard]] std::string GetActiveScenarioId()    const noexcept;
    [[nodiscard]] bool        IsSwitchPending()        const noexcept;

    /// Returns all registered scenario descriptors for the picker UI.
    [[nodiscard]] std::vector<const ScenarioDescriptor*> GetAllDescriptors() const;

private:
    // ── Internal scenario lifecycle ───────────────────────────────────────────

    /// Immediately activate the scenario with the given ID.
    /// Shuts down the current scenario first if one is active.
    bool ActivateScenario(std::string_view id);

    /// Shut down the currently active scenario.
    void DeactivateCurrentScenario();

    // ── State ─────────────────────────────────────────────────────────────────

    struct ScenarioEntry
    {
        std::string            id;
        ScenarioFactory        factory;
        ScenarioDescriptor     cachedDescriptor;  ///< From a probe instance at registration time
    };

    std::unordered_map<std::string, ScenarioEntry> m_registry;

    std::unique_ptr<IScenario> m_activeScenario;
    std::string                m_activeScenarioId;

    std::string                m_pendingSwitchId;  ///< Non-empty = deferred switch requested
    bool                       m_stopRequested = false;

    bool                       m_initialized = false;
};

} // namespace SagaSandbox