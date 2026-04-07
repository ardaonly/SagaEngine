/// @file DebugHud.h
/// @brief Composite debug HUD rendered with Dear ImGui each frame.
///
/// Layer  : Sandbox / UI
/// Purpose: DebugHud aggregates all standard diagnostic panels (frametime,
///          memory, profiler, network, ECS, scenario picker) and renders them
///          as a docked ImGui layout. Scenarios add their own panels on top.
///
/// UI boundary rule:
///   DebugHud lives ONLY in Apps/Sandbox/. It must never be included by
///   Engine/, Server/, or Editor/ code. It uses Dear ImGui headers directly
///   (via the sandbox's private include path).
///
/// Panel ownership:
///   DebugHud owns a fixed set of IDebugPanel instances (frametime, memory…).
///   Scenarios may call DebugHud::PushScenarioPanel() to add their own panel
///   for the duration of the scenario; it is removed on scenario shutdown.

#pragma once

#include "SagaSandbox/UI/DebugHud.h"
#include "SagaSandbox/UI/Panels/IDebugPanel.h"
#include <memory>
#include <string>
#include <vector>

namespace SagaSandbox
{

class ScenarioManager;

// ─── DebugHud ─────────────────────────────────────────────────────────────────

/// Manages the ImGui-based debug overlay for the sandbox.
class DebugHud
{
public:
    explicit DebugHud(const ScenarioManager& scenarioManager);
    ~DebugHud();

    DebugHud(const DebugHud&)            = delete;
    DebugHud& operator=(const DebugHud&) = delete;

    // ── Lifecycle ─────────────────────────────────────────────────────────────

    void Init();
    void Shutdown();

    // ── Per-frame rendering ───────────────────────────────────────────────────

    /// Call between ImGui::NewFrame() and ImGui::Render().
    void Render(float dt, uint64_t tick);

    // ── Visibility ────────────────────────────────────────────────────────────

    void SetVisible(bool visible) noexcept { m_visible = visible; }
    void ToggleVisible()          noexcept { m_visible = !m_visible; }
    [[nodiscard]] bool IsVisible() const   noexcept { return m_visible; }

    // ── Scenario panel injection ──────────────────────────────────────────────

    /// Add a panel for the currently active scenario. Ownership is taken.
    void PushScenarioPanel(std::unique_ptr<IDebugPanel> panel);

    /// Remove all scenario-injected panels (called on scenario shutdown).
    void ClearScenarioPanels();

private:
    // ── Internal render helpers ───────────────────────────────────────────────

    void RenderMainMenuBar();
    void RenderDockspace();
    void RenderFpsOverlay(float dt);
    void RenderAllPanels(float dt, uint64_t tick);

    // ── Built-in panels ───────────────────────────────────────────────────────

    std::vector<std::unique_ptr<IDebugPanel>> m_builtinPanels;

    // ── Scenario-injected panels ──────────────────────────────────────────────

    std::vector<std::unique_ptr<IDebugPanel>> m_scenarioPanels;

    // ── State ─────────────────────────────────────────────────────────────────

    const ScenarioManager& m_scenarioManager;  ///< Read-only reference for picker panel
    bool                   m_visible      = true;
    bool                   m_showFpsOverlay = true;
    bool                   m_initialized  = false;
};

} // namespace SagaSandbox