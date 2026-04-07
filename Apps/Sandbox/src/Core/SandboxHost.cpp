/// @file SandboxHost.cpp
/// @brief Main host application implementation.

#include "SagaSandbox/Core/SandboxHost.h"
#include "SagaSandbox/UI/DebugHud.h"
#include "SagaSandbox/Core/ScenarioRegistry.h"
#include <SagaEngine/Core/Log/Log.h>
#include <SagaEngine/Core/Time/Time.h>

#include <imgui.h>

namespace SagaSandbox
{

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

    // ── 2. Initialise ImGui (skip in headless mode) ───────────────────────────
    if (!m_config.headless)
    {
        InitImGui();
        m_debugHud = std::make_unique<DebugHud>(m_scenarioManager);
        m_debugHud->Init();
        m_debugHud->SetVisible(m_config.showHudOnStart);
    }

    // ── 3. Start the initial scenario (may be empty) ──────────────────────────
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
    m_scenarioManager.Update(dt, m_tickCounter);
    if (!m_config.headless && m_imguiReady)
        TickImGui(dt);
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

// ─── ImGui helpers ────────────────────────────────────────────────────────────

void SandboxHost::InitImGui()
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    // io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    ImGui::StyleColorsDark();

    // Platform/renderer backends — replace with your actual SDL_Window* handle.
    // ImGui_ImplSDL2_InitForOpenGL(window, glContext);
    // ImGui_ImplOpenGL3_Init("#version 460");

    m_imguiReady = true;
    LOG_INFO(kTag, "ImGui initialised (docking enabled).");
}

void SandboxHost::ShutdownImGui()
{
    // ImGui_ImplOpenGL3_Shutdown();
    // ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
    m_imguiReady = false;
    LOG_INFO(kTag, "ImGui shut down.");
}

void SandboxHost::TickImGui(float dt)
{
    // ImGui_ImplOpenGL3_NewFrame();
    // ImGui_ImplSDL2_NewFrame();
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
    // ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

} // namespace SagaSandbox