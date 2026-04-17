/// @file IScenario.h
/// @brief Base interface for all sandbox test scenarios.
///
/// Layer  : Sandbox / Core
/// Purpose: Every test scenario (InputProbe, ECSProbe, NetworkReplication…)
///          implements this interface. SandboxHost drives the lifecycle;
///          scenarios contain all test-specific setup, tick, and teardown logic.
///
/// Design contract:
///   - Scenarios are stateful. One instance = one active test session.
///   - Lifecycle is strictly linear: Init → Update (N) → Shutdown.
///   - A scenario must NOT hold global state beyond its own lifetime.
///   - Scenarios may register ImGui panels; the host will call RenderDebugUI()
///     once per frame inside an already-open ImGui frame.
///   - Scenarios MUST NOT touch RmlUi or Qt. Debug UI is ImGui-only here.
///
/// MMO note:
///   Scenarios that test authoritative simulation (NetworkReplication,
///   PredictionStress) should spin up an in-process mock server and client
///   using the real engine systems — not stubs. This exercises the real
///   network codepath without requiring a live server.

#pragma once

#include <SagaEngine/Render/Backend/IRenderBackend.h>
#include <SagaEngine/Render/Scene/Camera.h>
#include <SagaEngine/Render/Scene/RenderView.h>

#include <string_view>
#include <cstdint>

namespace SagaSandbox
{

// ─── Scenario Context ────────────────────────────────────────────────────────

/// Services available to scenarios during their lifetime.
/// Populated by SandboxHost before scenario activation.
struct ScenarioContext
{
    ::SagaEngine::Render::Backend::IRenderBackend* renderBackend = nullptr;
};

// ─── Scenario Metadata ────────────────────────────────────────────────────────

/// Describes a scenario for display in the launcher UI and logs.
struct ScenarioDescriptor
{
    std::string_view id;           ///< Unique machine-readable ID (e.g. "input_probe")
    std::string_view displayName;  ///< Human-readable name shown in HUD
    std::string_view category;     ///< "Input", "Network", "ECS", "Render", "Memory"…
    std::string_view description;  ///< One-sentence purpose
};

// ─── IScenario ────────────────────────────────────────────────────────────────

/// Pure interface for all sandbox scenarios.
class IScenario
{
public:
    virtual ~IScenario() = default;

    IScenario(const IScenario&)            = delete;
    IScenario& operator=(const IScenario&) = delete;

    // ── Identity ─────────────────────────────────────────────────────────────

    /// Returns static metadata for this scenario.
    [[nodiscard]] virtual const ScenarioDescriptor& GetDescriptor() const noexcept = 0;

    // ── Lifecycle ─────────────────────────────────────────────────────────────

    /// Called once after the scenario is selected and before the first Update.
    /// Engine subsystems (input, render, simulation…) are already running.
    /// Return false to abort before the scenario starts.
    [[nodiscard]] virtual bool OnInit() = 0;

    /// Called every simulation tick by SandboxHost.
    /// @param deltaTime  Seconds since last Update call.
    /// @param tick       Current simulation tick counter.
    virtual void OnUpdate(float deltaTime, uint64_t tick) = 0;

    /// Called once after the scenario ends (user switches or app closes).
    /// Must release all scenario-owned resources.
    virtual void OnShutdown() = 0;

    // ── Render integration ───────────────────────────────────────────────────

    /// Called once after construction, before OnInit. Provides access to
    /// host services (render backend, etc.). Override to store the pointer.
    virtual void OnAttach(const ScenarioContext& /*ctx*/) {}

    /// Called once per frame between BeginFrame and EndFrame.
    /// Fill outCamera and outView with the scene to render.
    /// Default: identity camera, empty view (nothing drawn).
    virtual void OnPrepareRender(
        ::SagaEngine::Render::Scene::Camera&     /*outCamera*/,
        ::SagaEngine::Render::Scene::RenderView& /*outView*/) {}

    // ── Debug UI ─────────────────────────────────────────────────────────────

    /// Called once per frame inside an active ImGui frame.
    /// Render scenario-specific panels (e.g. "Input Event Log", "Replication Stats").
    /// The host renders the global HUD separately; this is scenario-specific content.
    virtual void OnRenderDebugUI() {}

    // ── Optional hooks ────────────────────────────────────────────────────────

    /// Called when the application window is resized.
    virtual void OnResize(int /*width*/, int /*height*/) {}

    /// Called when the scenario is paused (e.g. debugger break, window loses focus).
    virtual void OnPause()  {}
    virtual void OnResume() {}

protected:
    IScenario() = default;
};

} // namespace SagaSandbox