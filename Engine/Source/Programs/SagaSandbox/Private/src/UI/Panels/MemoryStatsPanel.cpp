/// @file MemoryStatsPanel.cpp
/// @brief Sandbox HUD memory panel — reads MemoryTracker each frame.

#include "SagaSandbox/UI/Panels/MemoryStatsPanel.h"
#include <SagaEngine/Core/Memory/MemoryTracker.h>
#include <imgui.h>
#include <cstdio>

namespace SagaSandbox
{

void MemoryStatsPanel::Render(float /*dt*/, uint64_t /*tick*/)
{
    if (!ImGui::Begin(GetTitle().data(), &m_open))
    {
        ImGui::End();
        return;
    }

    // ─── Pull live data ───────────────────────────────────────────────────────

    const auto& tracker = SagaEngine::Core::MemoryTracker::Instance();
    const size_t totalBytes  = tracker.GetTotalAllocated();
    const size_t totalAllocs = tracker.GetAllocationCount();
    const float  totalMB     = static_cast<float>(totalBytes) / (1024.f * 1024.f);

    if (totalBytes > m_peakAllocated)
        m_peakAllocated = totalBytes;

    // ─── History ring buffer ─────────────────────────────────────────────────

    m_allocHistory[m_historyOffset] = totalMB;
    m_historyOffset = (m_historyOffset + 1) % kHistorySize;

    // ─── Render ───────────────────────────────────────────────────────────────

    char overlayBuf[64];
    std::snprintf(overlayBuf, sizeof(overlayBuf), "%.2f MB", totalMB);

    ImGui::PlotLines("##mem", m_allocHistory.data(), kHistorySize,
                     m_historyOffset, overlayBuf, 0.f, 512.f,
                     ImVec2(0, 60));

    ImGui::Separator();

    ImGui::Text("Live allocations : %zu", totalAllocs);
    ImGui::Text("Total heap       : %.3f MB", totalMB);
    ImGui::Text("Peak heap        : %.3f MB",
                static_cast<float>(m_peakAllocated) / (1024.f * 1024.f));

    if (ImGui::Button("Dump Leaks to Log"))
    {
        SagaEngine::Core::MemoryTracker::Instance().DumpLeaks();
    }

    ImGui::End();
}

} // namespace SagaSandbox