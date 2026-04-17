/// @file SandboxHost.cpp
/// @brief Main host application implementation.
///
/// Render backend integration (Phase 3):
///   OnInit   → InitRenderBackend() creates DiligentRenderBackend,
///              passes it to ScenarioManager via ScenarioContext.
///   OnUpdate → BeginFrame → scenario PrepareRender → Submit → EndFrame.
///   OnShutdown → ScenarioManager.Shutdown → ShutdownRenderBackend().
///
///   Scenarios own their meshes and materials. SandboxHost owns the backend
///   lifetime and the frame-level Begin/End/Submit cycle.

#include "SagaSandbox/Core/SandboxHost.h"
#include "SagaSandbox/UI/DebugHud.h"
#include "SagaSandbox/Core/ScenarioRegistry.h"
#include <SagaEngine/Core/Log/Log.h>
#include <SagaEngine/Core/Time/Time.h>
#include <SagaEngine/Render/Backend/Diligent/DiligentRenderBackend.h>
#include <SagaEngine/Render/Scene/Camera.h>
#include <SagaEngine/Render/Scene/RenderView.h>

#include <imgui.h>

// For extracting the OS-native handle from SDL_Window*.
#include <SDL2/SDL.h>
#include <SDL2/SDL_syswm.h>

#include <cstdio>
#include <string>

namespace SagaSandbox
{

namespace RB = SagaEngine::Render::Backend;

static constexpr const char* kTag = "SandboxHost";

// ─── Construction ─────────────────────────────────────────────────────────────

SandboxHost::SandboxHost(SandboxConfig config)
    : Application(config.windowTitle)
    , m_config(std::move(config))
{
}

SandboxHost::~SandboxHost() = default;

// ─── Application interface ────────────────────────────────────────────────────

void SandboxHost::OnInit()
{
    LOG_INFO(kTag, "SandboxHost initialising…");

    // ── 1. Register all self-registering scenarios ────────────────────────────
    ScenarioRegistry::RegisterAll(m_scenarioManager);
    LOG_INFO(kTag, "%zu scenario(s) registered.", m_scenarioManager.GetAllDescriptors().size());

    // ── 2. Initialise render backend (skip in headless mode) ──────────────────
    if (!m_config.headless)
    {
        if (!InitRenderBackend())
        {
            LOG_WARN(kTag, "Render backend unavailable — running without GPU.");
        }
    }

    // ── 3. Publish render backend to ScenarioManager ─────────────────────────
    ScenarioContext ctx{};
    ctx.renderBackend = m_renderBackend.get();  // null if headless or init failed
    m_scenarioManager.SetContext(ctx);

    // ── 4. Initialise ImGui (skip in headless mode) ───────────────────────────
    if (!m_config.headless)
    {
        InitImGui();
        m_debugHud = std::make_unique<DebugHud>(m_scenarioManager);
        m_debugHud->Init();
        m_debugHud->SetVisible(m_config.showHudOnStart);
    }

    // ── 5. Start the initial scenario (may be empty) ──────────────────────────
    if (!m_scenarioManager.Init(m_config.initialScenarioId))
    {
        LOG_ERROR(kTag, "Initial scenario '%s' failed to start.",
                  m_config.initialScenarioId.c_str());
        RequestClose();
        return;
    }

    LOG_INFO(kTag, "SandboxHost ready.");
}

void SandboxHost::OnUpdate()
{
    const float dt = SagaEngine::Core::Time::GetDeltaTime();
    m_tickCounter++;

    // ── GPU frame begin ───────────────────────────────────────────────────────
    if (m_renderBackend && m_renderBackend->IsInitialized())
        m_renderBackend->BeginFrame();

    // ── Scenarios ─────────────────────────────────────────────────────────────
    m_scenarioManager.Update(dt, m_tickCounter);

    // ── Scenario-driven render submission ────────────────────────────────────
    if (m_renderBackend && m_renderBackend->IsInitialized())
    {
        SagaEngine::Render::Scene::Camera     cam{};
        SagaEngine::Render::Scene::RenderView view{};
        m_scenarioManager.PrepareRender(cam, view);
        m_renderBackend->Submit(cam, view);
    }

    // ── ImGui ─────────────────────────────────────────────────────────────────
    if (!m_config.headless && m_imguiReady)
        TickImGui(dt);

    // ── GPU frame end ─────────────────────────────────────────────────────────
    if (m_renderBackend && m_renderBackend->IsInitialized())
        m_renderBackend->EndFrame();

    // ── FPS in title bar ──────────────────────────────────────────────────────
    if (!m_config.headless)
        UpdateFPSTitle(dt);
}

void SandboxHost::OnShutdown()
{
    LOG_INFO(kTag, "SandboxHost shutting down…");

    m_scenarioManager.Shutdown();

    if (!m_config.headless && m_imguiReady)
    {
        if (m_debugHud) { m_debugHud->Shutdown(); m_debugHud.reset(); }
        ShutdownImGui();
    }

    // Backend shuts down AFTER ImGui (ImGui may hold GPU resources in the
    // future once the ImGui→Diligent rendering path is wired).
    ShutdownRenderBackend();

    LOG_INFO(kTag, "SandboxHost shutdown complete.");
}

// ─── Public API ───────────────────────────────────────────────────────────────

void SandboxHost::SwitchScenario(std::string_view id)
{
    m_scenarioManager.RequestSwitch(id);
}

void SandboxHost::RequestExit()
{
    RequestClose();
}

// ─── Render backend helpers ──────────────────────────────────────────────────

bool SandboxHost::InitRenderBackend()
{
    auto* window = GetWindow();
    if (!window)
    {
        LOG_ERROR(kTag, "No IWindow available for render backend init.");
        return false;
    }

    // GetNativeHandle() returns SDL_Window* for the SDL backend.
    auto* sdlWindow = static_cast<SDL_Window*>(window->GetNativeHandle());
    if (!sdlWindow)
    {
        LOG_ERROR(kTag, "SDL_Window* is null — cannot extract OS handle.");
        return false;
    }

    // ── Extract the OS-native handle ──────────────────────────────────────────
    SDL_SysWMinfo wmInfo{};
    SDL_VERSION(&wmInfo.version);
    if (!SDL_GetWindowWMInfo(sdlWindow, &wmInfo))
    {
        LOG_ERROR(kTag, "SDL_GetWindowWMInfo failed: %s", SDL_GetError());
        return false;
    }

    void* nativeHandle = nullptr;
#if defined(_WIN32)
    nativeHandle = wmInfo.info.win.window;   // HWND
#elif defined(__linux__)
#   if defined(SDL_VIDEO_DRIVER_X11)
    nativeHandle = reinterpret_cast<void*>(wmInfo.info.x11.window);
#   elif defined(SDL_VIDEO_DRIVER_WAYLAND)
    nativeHandle = wmInfo.info.wl.surface;
#   endif
#elif defined(__APPLE__)
    nativeHandle = wmInfo.info.cocoa.window; // NSWindow*
#endif

    if (!nativeHandle)
    {
        LOG_ERROR(kTag, "Could not extract OS-native window handle.");
        return false;
    }

    // ── Create + initialize the backend ───────────────────────────────────────
    m_renderBackend = std::make_unique<RB::DiligentRenderBackend>(m_config.renderBackend);

    RB::SwapchainDesc scDesc{};
    scDesc.nativeWindow = nativeHandle;
    scDesc.width        = window->GetWidth();
    scDesc.height       = window->GetHeight();
    scDesc.vsync        = true;
    scDesc.hdr          = false;

    if (!m_renderBackend->Initialize(scDesc))
    {
        LOG_WARN(kTag, "DiligentRenderBackend::Initialize failed.");
        m_renderBackend.reset();
        return false;
    }

    LOG_INFO(kTag, "Render backend ready: %s (frame %llu)",
             std::string(RB::ToString(m_renderBackend->SelectedAPI())).c_str(),
             static_cast<unsigned long long>(m_renderBackend->FrameIndex()));

    // ── Wire resize callback ──────────────────────────────────────────────────
    // TODO: Wire SDLWindow::SetOnResize → m_renderBackend->OnResize.

    return true;
}

void SandboxHost::ShutdownRenderBackend()
{
    if (m_renderBackend)
    {
        m_renderBackend->Shutdown();
        m_renderBackend.reset();
        LOG_INFO(kTag, "Render backend shut down.");
    }
}

void SandboxHost::UpdateFPSTitle(float dt)
{
    if (m_config.fpsInTitleInterval <= 0) return;

    m_fpsAccumulator += dt;
    m_fpsFrameCount++;

    if (m_fpsFrameCount >= m_config.fpsInTitleInterval)
    {
        const float avgDt  = m_fpsAccumulator / static_cast<float>(m_fpsFrameCount);
        const float avgFps = (avgDt > 0.0f) ? (1.0f / avgDt) : 0.0f;

        char title[256];
        if (m_renderBackend && m_renderBackend->IsInitialized())
        {
            std::snprintf(title, sizeof(title),
                          "%s | %.1f FPS (%.2f ms) | %s | frame %llu",
                          m_config.windowTitle.c_str(),
                          avgFps, avgDt * 1000.0f,
                          std::string(RB::ToString(m_renderBackend->SelectedAPI())).c_str(),
                          static_cast<unsigned long long>(m_renderBackend->FrameIndex()));
        }
        else
        {
            std::snprintf(title, sizeof(title),
                          "%s | %.1f FPS (%.2f ms) | no GPU",
                          m_config.windowTitle.c_str(),
                          avgFps, avgDt * 1000.0f);
        }

        // Set the window title via the IWindow interface. SDLWindow
        // implements SetTitle, but IWindow doesn't expose it. Use SDL
        // directly since we know the platform backend.
        auto* sdlWindow = static_cast<SDL_Window*>(GetWindow()->GetNativeHandle());
        if (sdlWindow)
            SDL_SetWindowTitle(sdlWindow, title);

        m_fpsAccumulator = 0.0f;
        m_fpsFrameCount  = 0;
    }
}

// ─── ImGui helpers ────────────────────────────────────────────────────────────

void SandboxHost::InitImGui()
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    // io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    ImGui::StyleColorsDark();

    // Platform/renderer backends — ImGui→Diligent rendering is deferred to
    // Phase 2. For now, ImGui generates draw data but nothing submits it to
    // the GPU. The HUD and scenario picker still function in-memory.
    //
    // Phase 2 plan:
    //   - Add diligent-tools conan dep (provides ImGuiImplDiligent)
    //   - ImGui_ImplSDL2_InitForOther(sdlWindow)
    //   - ImGuiImplDiligent::Init(device, swapchainFormat)
    //   - Wire TickImGui → ImGuiImplDiligent::RenderDrawData

    m_imguiReady = true;
    LOG_INFO(kTag, "ImGui initialised (render backend deferred to Phase 2).");
}

void SandboxHost::ShutdownImGui()
{
    // Phase 2: ImGui_ImplDiligent::Shutdown()
    // Phase 2: ImGui_ImplSDL2_Shutdown()
    ImGui::DestroyContext();
    m_imguiReady = false;
    LOG_INFO(kTag, "ImGui shut down.");
}

void SandboxHost::TickImGui(float dt)
{
    // Phase 2: ImGui_ImplDiligent::NewFrame()
    // Phase 2: ImGui_ImplSDL2_NewFrame()
    ImGui::NewFrame();

    // ── Toggle HUD with F1 ────────────────────────────────────────────────────
    if (ImGui::IsKeyPressed(ImGuiKey_F1))
    {
        m_debugHud->ToggleVisible();
    }

    // ── Render HUD (scenario picker, panels, overlays) ────────────────────────
    if (m_debugHud && m_debugHud->IsVisible())
    {
        m_debugHud->Render(dt, m_tickCounter);
    }

    // ── Render scenario-specific debug UI ─────────────────────────────────────
    m_scenarioManager.RenderDebugUI();

    ImGui::Render();
    // Phase 2: ImGui_ImplDiligent::RenderDrawData(ImGui::GetDrawData())
}

} // namespace SagaSandbox
