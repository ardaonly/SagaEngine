/// @file RenderClientApp.h
/// @brief Reusable composition of RenderWorld + NetEntityBinding + RenderSubsystem.
///
/// Layer  : Runtime / Client
/// Purpose: Every client-facing binary (demo, editor, headless test tool,
///          replay player) needs the same three pieces wired the same way:
///            - a RenderWorld for drawables,
///            - a NetEntityBinding to feed replicated state into it,
///            - a RenderSubsystem to cull and submit per camera.
///          This class owns those three in one place so the hosting
///          application only has to hand in a concrete IRenderBackend and
///          decide which default cameras to create. Nothing about
///          windowing, input, platform entry points, or SDL lives here.
///
/// Responsibilities:
///   - Owns: RenderWorld, NetEntityBinding, RenderSubsystem.
///   - Borrows: IRenderBackend (reference; caller owns the lifetime).
///   - Provides: Initialize / Tick / Shutdown lifecycle.
///   - Exposes: accessors so the host can register custom cameras, push
///     replicated state through the bridge, etc.
///
/// Deliberate non-responsibilities:
///   - No main() entry point, no platform hooks.
///   - No game loop — the host drives Tick().
///   - No window, no SDL, no input.
///   - No shader / mesh / material loading (backend's concern).
///
/// Clients built on top:
///   - Apps/RenderClientSmokeTest (headless facade contract verification)
///   - Apps/Editor's viewport runtime (future)
///   - Headless render-diff tool (future)
///   - Replay player (future)

#pragma once

#include "SagaEngine/Render/Backend/IRenderBackend.h"
#include "SagaEngine/Render/Bridge/NetEntityBinding.h"
#include "SagaEngine/Render/LOD/LODSelector.h"
#include "SagaEngine/Render/RenderSubsystem.h"
#include "SagaEngine/Render/Scene/Camera.h"
#include "SagaEngine/Render/Scene/FrameContext.h"
#include "SagaEngine/Render/World/RenderWorld.h"

#include <cstdint>

namespace SagaEngine::Runtime::Client
{

namespace Render = ::SagaEngine::Render;

// ─── Configuration ────────────────────────────────────────────────────

/// Flags controlling which default cameras the facade creates on Initialize.
/// Every flag independently toggles one canonical camera slot. The host
/// is free to add more cameras afterwards via Subsystem().AddCamera().
struct DefaultCameras
{
    bool main    = true;    ///< Primary world camera.
    bool shadow  = false;   ///< One placeholder shadow-cascade camera.
    bool minimap = false;
    bool debug   = false;
};

/// One-shot configuration for RenderClientApp::Initialize.
struct RenderClientConfig
{
    Render::Backend::SwapchainDesc  swapchain{};          ///< Forwarded to backend.Initialize.
    DefaultCameras                  defaultCameras{};
    Render::LOD::QualityPreset      quality =
        Render::LOD::QualityPreset::High;

    /// When set, forwarded to RenderSubsystem::SetLodLookup before any
    /// Tick(). Typically the backend's mesh-table lookup.
    Render::Culling::LodThresholdLookupFn lodLookup;
};

// ─── Facade ───────────────────────────────────────────────────────────

/// Client-side render facade. One per process.
class RenderClientApp
{
public:
    /// The caller keeps `backend` alive for the facade's entire lifetime.
    explicit RenderClientApp(Render::Backend::IRenderBackend& backend) noexcept;

    ~RenderClientApp();

    RenderClientApp(const RenderClientApp&)            = delete;
    RenderClientApp& operator=(const RenderClientApp&) = delete;

    // ── Lifecycle ────────────────────────────────────────────────

    /// Initialise the backend, install LOD lookup, create default cameras.
    /// Returns false if the backend fails to initialise; in that case the
    /// facade is left in an uninitialised state and Tick() is a no-op.
    [[nodiscard]] bool Initialize(const RenderClientConfig& cfg);

    /// Run one frame. Safe to call before Initialize (no-op).
    void Tick(const Render::Scene::FrameContext& ctx);

    /// Destroy cameras, clear the bridge, shut the backend down.
    /// Idempotent; safe to call multiple times.
    void Shutdown();

    [[nodiscard]] bool IsInitialized() const noexcept { return m_Initialized; }

    // ── Accessors (host drives these) ────────────────────────────

    [[nodiscard]] Render::World::RenderWorld&         World()     noexcept { return m_World; }
    [[nodiscard]] Render::Bridge::NetEntityBinding&   Bridge()    noexcept { return m_Bridge; }
    [[nodiscard]] Render::RenderSubsystem&            Subsystem() noexcept { return m_Subsystem; }
    [[nodiscard]] Render::Backend::IRenderBackend&    Backend()   noexcept { return *m_Backend; }

    [[nodiscard]] const Render::World::RenderWorld&         World()     const noexcept { return m_World; }
    [[nodiscard]] const Render::Bridge::NetEntityBinding&   Bridge()    const noexcept { return m_Bridge; }
    [[nodiscard]] const Render::RenderSubsystem&            Subsystem() const noexcept { return m_Subsystem; }

    // ── Default camera handles ───────────────────────────────────

    /// Handle for each default camera. `kInvalid` if the corresponding
    /// flag was false in `DefaultCameras`.
    [[nodiscard]] Render::Scene::CameraId MainCameraId()    const noexcept { return m_MainCam; }
    [[nodiscard]] Render::Scene::CameraId ShadowCameraId()  const noexcept { return m_ShadowCam; }
    [[nodiscard]] Render::Scene::CameraId MinimapCameraId() const noexcept { return m_MinimapCam; }
    [[nodiscard]] Render::Scene::CameraId DebugCameraId()   const noexcept { return m_DebugCam; }

private:
    // Member declaration order matters — NetEntityBinding and RenderSubsystem
    // both take a RenderWorld reference, so it must be declared first.

    Render::Backend::IRenderBackend*  m_Backend    = nullptr;
    Render::World::RenderWorld        m_World;
    Render::Bridge::NetEntityBinding  m_Bridge;
    Render::RenderSubsystem           m_Subsystem;

    bool                              m_Initialized = false;

    Render::Scene::CameraId m_MainCam    = Render::Scene::CameraId::kInvalid;
    Render::Scene::CameraId m_ShadowCam  = Render::Scene::CameraId::kInvalid;
    Render::Scene::CameraId m_MinimapCam = Render::Scene::CameraId::kInvalid;
    Render::Scene::CameraId m_DebugCam   = Render::Scene::CameraId::kInvalid;
};

} // namespace SagaEngine::Runtime::Client
