/// @file SandboxHost.h
/// @brief The central host application for all sandbox scenarios.
///
/// Layer  : Sandbox / Core
/// Purpose: SandboxHost is the single coherent entry point for the entire
///          sandbox product. It inherits from Application (engine core),
///          owns the ScenarioManager and DebugHud, and drives the main loop.
///
/// Architecture invariant:
///   SandboxHost is the ONLY class in the Sandbox layer that touches
///   Application, ScenarioManager, and DebugHud simultaneously.
///   All three live here and nowhere else.
///
/// UI separation:
///   - SandboxHost initialises ImGui exclusively for sandbox use.
///   - RmlUi (game UI) and Qt (editor) are never initialised here.
///   - ImGui context is destroyed on SandboxHost::OnShutdown.
///
/// MMO readiness:
///   SandboxHost can run in "headless" mode (no window, no ImGui) for
///   server-side scenario testing (PredictionStress, NetworkReplication).
///   Set SandboxConfig::headless = true before calling Run().

#pragma once

#include "SagaSandbox/Core/ScenarioManager.h"
#include "SagaSandbox/Core/SandboxConfig.h"
#include "SagaSandbox/Core/SandboxHost.h"
#include <SagaEngine/Core/Application/Application.h>
#include <memory>
#include <string>

namespace SagaSandbox
{

class DebugHud;

// ─── SandboxHost ──────────────────────────────────────────────────────────────

/// Main host application. One instance per process, never shared.
class SandboxHost final : public SagaEngine::Core::Application
{
public:
    /// @param config   Runtime configuration (scenario id, headless mode, etc.)
    explicit SandboxHost(SandboxConfig config);
    ~SandboxHost() override;

    SandboxHost(const SandboxHost&)            = delete;
    SandboxHost& operator=(const SandboxHost&) = delete;

    // ── Application interface ─────────────────────────────────────────────────

    void OnInit()                override;
    void OnUpdate(float dt)      override;
    void OnShutdown()            override;

    // ── Public API ────────────────────────────────────────────────────────────

    /// Switch the active scenario at the next Update boundary.
    void SwitchScenario(std::string_view id);

    /// Request a clean shutdown of the sandbox (stops the main loop).
    void RequestExit();

    [[nodiscard]] const SandboxConfig&    GetConfig()          const noexcept { return m_config; }
    [[nodiscard]] const ScenarioManager&  GetScenarioManager() const noexcept { return m_scenarioManager; }

private:
    // ── Internal helpers ──────────────────────────────────────────────────────

    /// Initialise ImGui backend for the current platform window.
    void InitImGui();

    /// Tear down ImGui backend.
    void ShutdownImGui();

    /// Begin a new ImGui frame, tick HUD and scenario UI, then render.
    void TickImGui(float dt);

    // ── State ─────────────────────────────────────────────────────────────────

    SandboxConfig              m_config;
    ScenarioManager            m_scenarioManager;
    std::unique_ptr<DebugHud>  m_debugHud;

    uint64_t                   m_tickCounter  = 0;
    bool                       m_imguiReady   = false;
};

} // namespace SagaSandbox