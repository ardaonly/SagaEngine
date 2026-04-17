/// @file RenderClientApp.cpp
/// @brief Composition of the client-side render pipeline. Pure wire-up.

#include "SagaRuntime/Client/RenderClientApp.h"

namespace SagaEngine::Runtime::Client
{

namespace R = ::SagaEngine::Render;

namespace
{

/// Default seed for every canonical camera. The host is expected to
/// overwrite view / projection before the first Tick; placeholders here
/// make the subsystem happy in case it runs before the host configures.
R::Scene::Camera MakePlaceholderCamera() noexcept
{
    R::Scene::Camera cam{};
    cam.view       = ::SagaEngine::Math::Mat4::Identity();
    cam.projection = ::SagaEngine::Math::Mat4::Identity();
    cam.filter     = R::World::kVisibilityAll;
    return cam;
}

} // namespace

// ─── Construction ─────────────────────────────────────────────────────

RenderClientApp::RenderClientApp(R::Backend::IRenderBackend& backend) noexcept
    : m_Backend(&backend)
    , m_Bridge(m_World)
    , m_Subsystem(m_World, backend)
{}

RenderClientApp::~RenderClientApp()
{
    Shutdown();
}

// ─── Initialize ───────────────────────────────────────────────────────

bool RenderClientApp::Initialize(const RenderClientConfig& cfg)
{
    if (m_Initialized) return true;
    if (m_Backend == nullptr) return false;

    if (!m_Backend->Initialize(cfg.swapchain))
        return false;

    if (cfg.lodLookup)
        m_Subsystem.SetLodLookup(cfg.lodLookup);
    m_Subsystem.SetQualityPreset(cfg.quality);

    if (cfg.defaultCameras.shadow)
        m_ShadowCam  = m_Subsystem.AddCamera(R::CameraRole::Shadow,  MakePlaceholderCamera(), "default.shadow");
    if (cfg.defaultCameras.main)
        m_MainCam    = m_Subsystem.AddCamera(R::CameraRole::Main,    MakePlaceholderCamera(), "default.main");
    if (cfg.defaultCameras.minimap)
        m_MinimapCam = m_Subsystem.AddCamera(R::CameraRole::Minimap, MakePlaceholderCamera(), "default.minimap");
    if (cfg.defaultCameras.debug)
        m_DebugCam   = m_Subsystem.AddCamera(R::CameraRole::Debug,   MakePlaceholderCamera(), "default.debug");

    m_Initialized = true;
    return true;
}

// ─── Tick ─────────────────────────────────────────────────────────────

void RenderClientApp::Tick(const R::Scene::FrameContext& ctx)
{
    if (!m_Initialized) return;
    m_Subsystem.ExecuteFrame(ctx);
}

// ─── Shutdown ─────────────────────────────────────────────────────────

void RenderClientApp::Shutdown()
{
    if (!m_Initialized) return;

    // Drop replicated mappings before the backend tears its resources
    // down — any stale bridge handle that survives teardown is a bug.
    m_Bridge.Clear();

    // Remove cameras so the backend sees a fully drained subsystem if
    // it wants to assert on empty state.
    for (auto* id : {&m_ShadowCam, &m_MainCam, &m_MinimapCam, &m_DebugCam})
    {
        if (*id != R::Scene::CameraId::kInvalid)
        {
            m_Subsystem.RemoveCamera(*id);
            *id = R::Scene::CameraId::kInvalid;
        }
    }

    if (m_Backend != nullptr)
        m_Backend->Shutdown();

    m_Initialized = false;
}

} // namespace SagaEngine::Runtime::Client
