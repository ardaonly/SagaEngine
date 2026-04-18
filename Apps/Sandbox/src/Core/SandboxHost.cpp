/// @file SandboxHost.cpp
/// @brief Main host application implementation.
///
/// Render backend integration:
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
    ctx.requestClose  = [this]() { RequestClose(); };
    m_scenarioManager.SetContext(ctx);

    // ── 4. Initialise ImGui (skip in headless mode) ───────────────────────────
    // NOTE: ImGui tick + render are DISABLED until Phase 2 wires
    //       ImGui_ImplSDL2 + ImGui_ImplDiligent.  Without a platform/
    //       renderer backend, ImGui::NewFrame() crashes on the first
    //       frame and ImGui::Render() produces draw data nobody submits.
    //       InitImGui() still runs so the context exists for future use.
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

    // ── ImGui overlay (rendered after scene, before Present) ───────────────
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

    // ── Extract the OS-native handle via the platform layer ──────────────────
    void* nativeHandle = window->GetOSNativeHandle();
    if (!nativeHandle)
    {
        LOG_ERROR(kTag, "Could not obtain OS-native window handle.");
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

    // ── Tell the window that the RHI owns presentation ───────────────────────
    // SDL_UpdateWindowSurface conflicts with the D3D12/Vulkan swap chain;
    // skipping it prevents an immediate crash on the first Present().
    window->SetRHIOwnsPresent(true);

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

        GetWindow()->SetTitle(title);

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

    // Set display size so ImGui::NewFrame() doesn't assert on zero dimensions.
    auto* window = GetWindow();
    if (window)
    {
        io.DisplaySize.x = static_cast<float>(window->GetWidth());
        io.DisplaySize.y = static_cast<float>(window->GetHeight());
    }

    // Initialize the Diligent-based ImGui renderer (PSO, font atlas, etc.).
    if (m_renderBackend && m_renderBackend->IsInitialized())
    {
        if (!m_renderBackend->InitImGuiRendering())
        {
            LOG_WARN(kTag, "ImGui GPU renderer init failed — HUD will be invisible.");
        }
    }

    m_imguiReady = true;
    LOG_INFO(kTag, "ImGui initialised.");
}

void SandboxHost::ShutdownImGui()
{
    if (m_renderBackend)
        m_renderBackend->ShutdownImGuiRendering();

    ImGui::DestroyContext();
    m_imguiReady = false;
    LOG_INFO(kTag, "ImGui shut down.");
}

void SandboxHost::TickImGui(float dt)
{
    // Update display size every frame (handles resize).
    auto* window = GetWindow();
    if (window)
    {
        ImGuiIO& io = ImGui::GetIO();
        io.DisplaySize.x = static_cast<float>(window->GetWidth());
        io.DisplaySize.y = static_cast<float>(window->GetHeight());
        io.DeltaTime     = dt > 0.0f ? dt : (1.0f / 60.0f);
    }

    ImGui::NewFrame();

    // ── Render HUD (scenario picker, panels, overlays) ────────────────────────
    if (m_debugHud && m_debugHud->IsVisible())
    {
        m_debugHud->Render(dt, m_tickCounter);
    }

    // ── Render scenario-specific debug UI ─────────────────────────────────────
    m_scenarioManager.RenderDebugUI();

    ImGui::Render();

    // Submit ImGui draw data to the GPU.
    if (m_renderBackend && m_renderBackend->IsInitialized())
    {
        m_renderBackend->RenderImGuiDrawData(ImGui::GetDrawData());
    }
}

} // namespace SagaSandbox
