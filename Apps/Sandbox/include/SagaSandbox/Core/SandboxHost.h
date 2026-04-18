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
///   Application, ScenarioManager, DebugHud, and DiligentRenderBackend
///   simultaneously. All four live here and nowhere else.
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
#include <SagaEngine/Core/Application/Application.h>
#include <SagaEngine/Render/Backend/Diligent/DiligentRenderBackend.h>
#include <memory>
#include <string>

namespace SagaSandbox
{

class DebugHud;

// ─── SandboxHost ──────────────────────────────────────────────────────────────

/// Main host application. One instance per process, never shared.
class SandboxHost final : public Saga::Application
{
public:
    /// @param config   Runtime configuration (scenario id, headless mode, etc.)
    explicit SandboxHost(SandboxConfig config);
    ~SandboxHost() override;

    SandboxHost(const SandboxHost&)            = delete;
    SandboxHost& operator=(const SandboxHost&) = delete;

    // ── Application interface ─────────────────────────────────────────────────

    void OnInit()                override;
    void OnUpdate()              override;
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

    /// Initialise the Diligent render backend from the current IWindow.
    /// Delegates OS-native handle extraction to IWindow::GetOSNativeHandle().
    /// Returns false if the backend could not be created (no GPU, headless, etc.).
    bool InitRenderBackend();

    /// Shutdown the Diligent render backend (idempotent).
    void ShutdownRenderBackend();

    /// Update the window title with FPS / frame-time stats.
    void UpdateFPSTitle(float dt);

    // ── State ─────────────────────────────────────────────────────────────────

    SandboxConfig              m_config;
    ScenarioManager            m_scenarioManager;
    std::unique_ptr<DebugHud>  m_debugHud;

    /// Diligent GPU backend. Created in OnInit when headless == false.
    /// Null until InitRenderBackend succeeds. Scenarios may query this
    /// for diagnostic readout (SelectedAPI, FrameIndex, etc.) but do NOT
    /// own it or shut it down — that is SandboxHost's job exclusively.
    std::unique_ptr<SagaEngine::Render::Backend::DiligentRenderBackend> m_renderBackend;

    uint64_t                   m_tickCounter    = 0;
    float                      m_fpsAccumulator = 0.0f;
    int                        m_fpsFrameCount  = 0;
    bool                       m_imguiReady     = false;
};

} // namespace SagaSandbox