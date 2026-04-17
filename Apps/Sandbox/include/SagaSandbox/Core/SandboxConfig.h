/// @file SandboxConfig.h
/// @brief Runtime configuration passed to SandboxHost at construction time.
///
/// Layer  : Sandbox / Core
/// Purpose: All behavioural knobs for the sandbox in one place.
///          Launchers build a SandboxConfig and pass it to SandboxHost.
///          Config is const after construction — no runtime mutation.

#pragma once

#include "SagaEngine/Render/Backend/Diligent/DiligentRenderBackend.h"

#include <string>
#include <cstdint>

namespace SagaSandbox
{

struct SandboxConfig
{
    // ── Scenario selection ────────────────────────────────────────────────────

    /// ID of the scenario to auto-launch. Empty = show picker only.
    std::string initialScenarioId;

    // ── Window / display ─────────────────────────────────────────────────────

    std::string windowTitle  = "SagaEngine Sandbox";
    int         windowWidth  = 1600;
    int         windowHeight = 900;

    /// Run without window or rendering (for headless server-side scenarios).
    bool headless = false;

    // ── Render backend ───────────────────────────────────────────────────────

    /// Diligent backend configuration (API preference, validation, clear
    /// colour, etc.). Only used when headless == false. When headless is
    /// true, no backend is created.
    SagaEngine::Render::Backend::DiligentBackendConfig renderBackend;

    /// Update the window title with FPS / frame-time every N frames.
    /// 0 disables the FPS title update. Default: every 30 frames.
    int fpsInTitleInterval = 30;

    // ── HUD ───────────────────────────────────────────────────────────────────

    bool showHudOnStart  = true;   ///< HUD visible on first frame
    bool showFpsOverlay  = true;   ///< Frametime overlay in top-right corner
    int  targetTickRate  = 60;     ///< Fixed-step rate used by SimulationTick

    // ── Observability ─────────────────────────────────────────────────────────

    bool captureMemoryStats  = true;   ///< Feed MemoryTracker data into HUD
    bool captureProfilerData = true;   ///< Feed Profiler samples into HUD

    // ── Network (for network-facing scenarios) ────────────────────────────────

    std::string mockServerAddress = "127.0.0.1";
    uint16_t    mockServerPort    = 17200;

    // ── Misc ──────────────────────────────────────────────────────────────────

    /// Log scenario lifecycle events (init/switch/shutdown) to engine log.
    bool verboseScenarioLog = true;
};

} // namespace SagaSandbox