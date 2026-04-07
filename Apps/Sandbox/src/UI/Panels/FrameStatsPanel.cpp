/// @file FrameStatsPanel.cpp
/// @brief Frame time sparkline + tick/entity counters.

#include "SagaSandbox/UI/Panels/FrameStatsPanel.h"
#include <SagaEngine/Simulation/WorldState.h>
#include <imgui.h>
#include <algorithm>
#include <cstdio>

namespace SagaSandbox
{

void FrameStatsPanel::Render(float dt, uint64_t tick)
{
    if (!ImGui::Begin(GetTitle().data(), &m_open))
    {
        ImGui::End();
        return;
    }

    // ─── Frame time history ───────────────────────────────────────────────────

    const float ms = dt * 1000.f;
    m_frameTimes[m_historyOffset] = ms;
    m_historyOffset = (m_historyOffset + 1) % kHistorySize;
    m_worstFrameMs  = std::max(m_worstFrameMs, ms);

    char overlay[48];
    std::snprintf(overlay, sizeof(overlay), "%.2f ms", ms);

    // Colour the sparkline red when > 16.67 ms (60 fps threshold)
    const ImVec4 lineCol = (ms > 16.67f)
        ? ImVec4(1.f, 0.3f, 0.3f, 1.f)
        : ImVec4(0.4f, 0.9f, 0.4f, 1.f);

    ImGui::PushStyleColor(ImGuiCol_PlotLines, lineCol);
    ImGui::PlotLines("##ft", m_frameTimes.data(), kHistorySize,
                     m_historyOffset, overlay, 0.f, 33.f, ImVec2(0, 60));
    ImGui::PopStyleColor();

    // ─── Stats ────────────────────────────────────────────────────────────────

    ImGui::Separator();
    ImGui::Text("Frame time : %.3f ms  (%.1f fps)", ms, ms > 0.f ? 1000.f / ms : 0.f);
    ImGui::Text("Worst      : %.3f ms", m_worstFrameMs);
    ImGui::Text("Tick       : %llu", (unsigned long long)tick);

    if (ImGui::Button("Reset worst"))
        m_worstFrameMs = 0.f;

    ImGui::End();
}

} // namespace SagaSandbox