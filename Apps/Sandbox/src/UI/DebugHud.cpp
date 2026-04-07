/// @file DebugHud.cpp
/// @brief Composite ImGui HUD — initialises builtin panels and drives render.

#include "SagaSandbox/UI/DebugHud.h"
#include "SagaSandbox/Core/ScenarioManager.h"
#include "SagaSandbox/UI/Panels/FrameStatsPanel.h"
#include "SagaSandbox/UI/Panels/ProfilerPanel.h"
#include "SagaSandbox/UI/Panels/MemoryStatsPanel.h"
#include "SagaSandbox/UI/Panels/NetworkStatsPanel.h"
#include "SagaSandbox/UI/Panels/EventLogPanel.h"
#include "SagaSandbox/UI/Panels/ScenarioPickerPanel.h"
#include <imgui.h>

namespace SagaSandbox
{

// ─── Construction ─────────────────────────────────────────────────────────────

DebugHud::DebugHud(const ScenarioManager& scenarioManager)
    : m_scenarioManager(scenarioManager)
{}

DebugHud::~DebugHud() = default;

// ─── Lifecycle ────────────────────────────────────────────────────────────────

void DebugHud::Init()
{
    if (m_initialized) return;

    // Builtin panels — order determines default docking priority
    m_builtinPanels.push_back(std::make_unique<FrameStatsPanel>());
    m_builtinPanels.push_back(std::make_unique<MemoryStatsPanel>());
    m_builtinPanels.push_back(std::make_unique<ProfilerPanel>());
    m_builtinPanels.push_back(std::make_unique<NetworkStatsPanel>());
    m_builtinPanels.push_back(
        std::make_unique<ScenarioPickerPanel>(
            const_cast<ScenarioManager&>(m_scenarioManager)
        )
    );
    m_builtinPanels.push_back(std::make_unique<EventLogPanel>());

    m_initialized = true;
}

void DebugHud::Shutdown()
{
    m_scenarioPanels.clear();
    m_builtinPanels.clear();
    m_initialized = false;
}

// ─── Per-frame rendering ──────────────────────────────────────────────────────

void DebugHud::Render(float dt, uint64_t tick)
{
    if (!m_visible || !m_initialized) return;

    RenderMainMenuBar();
    RenderDockspace();

    if (m_showFpsOverlay)
        RenderFpsOverlay(dt);

    RenderAllPanels(dt, tick);
}

// ─── Scenario panel injection ─────────────────────────────────────────────────

void DebugHud::PushScenarioPanel(std::unique_ptr<IDebugPanel> panel)
{
    m_scenarioPanels.push_back(std::move(panel));
}

void DebugHud::ClearScenarioPanels()
{
    m_scenarioPanels.clear();
}

// ─── Internal helpers ─────────────────────────────────────────────────────────

void DebugHud::RenderMainMenuBar()
{
    if (!ImGui::BeginMainMenuBar()) return;

    if (ImGui::BeginMenu("Sandbox"))
    {
        if (ImGui::MenuItem("Toggle HUD", "F1"))
            ToggleVisible();

        ImGui::MenuItem("FPS Overlay", nullptr, &m_showFpsOverlay);

        ImGui::Separator();

        if (ImGui::MenuItem("Stop Scenario"))
            const_cast<ScenarioManager&>(m_scenarioManager).RequestStop();

        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Panels"))
    {
        for (auto& panel : m_builtinPanels)
        {
            bool open = panel->IsOpen();
            if (ImGui::MenuItem(panel->GetTitle().data(), nullptr, open))
            {
                if (open) panel->Close();
                // re-open: panels don't have a dedicated Open() — toggle via
                // the menu just closes them; user can re-enable via Window menu
            }
        }
        ImGui::EndMenu();
    }

    // Right-aligned: show active scenario name
    const std::string activeId = m_scenarioManager.GetActiveScenarioId();
    if (!activeId.empty())
    {
        const float textWidth = ImGui::CalcTextSize(activeId.c_str()).x + 16.f;
        ImGui::SetCursorPosX(ImGui::GetContentRegionMax().x - textWidth);
        ImGui::TextColored(ImVec4(0.4f, 1.f, 0.4f, 1.f), "%s", activeId.c_str());
    }

    ImGui::EndMainMenuBar();
}

void DebugHud::RenderDockspace()
{
    // Full-screen dockspace beneath the menu bar
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->WorkPos);
    ImGui::SetNextWindowSize(vp->WorkSize);
    ImGui::SetNextWindowViewport(vp->ID);

    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoDocking          |
        ImGuiWindowFlags_NoTitleBar         |
        ImGuiWindowFlags_NoCollapse         |
        ImGuiWindowFlags_NoResize           |
        ImGuiWindowFlags_NoMove             |
        ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoNavFocus         |
        ImGuiWindowFlags_NoBackground;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);

    ImGui::Begin("##dockspace_root", nullptr, flags);
    ImGui::PopStyleVar(3);

    const ImGuiID dockId = ImGui::GetID("SandboxDock");
    ImGui::DockSpace(dockId, ImVec2(0, 0), ImGuiDockNodeFlags_PassthruCentralNode);

    ImGui::End();
}

void DebugHud::RenderFpsOverlay(float dt)
{
    constexpr float kPad = 10.f;
    const ImGuiViewport* vp = ImGui::GetMainViewport();

    ImGui::SetNextWindowPos(
        ImVec2(vp->WorkPos.x + vp->WorkSize.x - kPad,
               vp->WorkPos.y + kPad),
        ImGuiCond_Always,
        ImVec2(1.f, 0.f)   // anchor top-right
    );
    ImGui::SetNextWindowBgAlpha(0.55f);

    ImGuiWindowFlags overlayFlags =
        ImGuiWindowFlags_NoDecoration    |
        ImGuiWindowFlags_AlwaysAutoResize|
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoFocusOnAppearing |
        ImGuiWindowFlags_NoNav           |
        ImGuiWindowFlags_NoMove;

    if (ImGui::Begin("##fps_overlay", nullptr, overlayFlags))
    {
        const float ms  = dt * 1000.f;
        const float fps = ms > 0.f ? 1000.f / ms : 0.f;

        if (fps < 30.f)
            ImGui::TextColored(ImVec4(1, 0.3f, 0.3f, 1), "%.0f fps  %.2f ms", fps, ms);
        else if (fps < 55.f)
            ImGui::TextColored(ImVec4(1, 0.8f, 0.f, 1), "%.0f fps  %.2f ms", fps, ms);
        else
            ImGui::Text("%.0f fps  %.2f ms", fps, ms);
    }
    ImGui::End();
}

void DebugHud::RenderAllPanels(float dt, uint64_t tick)
{
    for (auto& panel : m_builtinPanels)
    {
        if (panel->IsOpen())
            panel->Render(dt, tick);
    }

    for (auto& panel : m_scenarioPanels)
    {
        if (panel->IsOpen())
            panel->Render(dt, tick);
    }
}

} // namespace SagaSandbox