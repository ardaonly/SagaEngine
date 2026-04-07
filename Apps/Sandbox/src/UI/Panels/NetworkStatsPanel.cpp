/// @file NetworkStatsPanel.cpp

#include "SagaSandbox/UI/Panels/NetworkStatsPanel.h"
#include <imgui.h>
#include <cstdio>

namespace SagaSandbox
{

void NetworkStatsPanel::Render(float /*dt*/, uint64_t /*tick*/)
{
    if (!ImGui::Begin(GetTitle().data(), &m_open))
    {
        ImGui::End();
        return;
    }

    // ─── History update ───────────────────────────────────────────────────────

    m_rttHistory[m_historyOffset]  = m_channel.rttMs;
    m_lossHistory[m_historyOffset] = m_channel.packetLossRate * 100.f;
    m_historyOffset = (m_historyOffset + 1) % kHistorySize;

    // ─── Channel section ──────────────────────────────────────────────────────

    ImGui::SeparatorText("Reliable Channel");

    char rttOverlay[32];
    std::snprintf(rttOverlay, sizeof(rttOverlay), "RTT %.1f ms", m_channel.rttMs);
    ImGui::PlotLines("##rtt", m_rttHistory.data(), kHistorySize,
                     m_historyOffset, rttOverlay, 0.f, 200.f, ImVec2(0, 50));

    char lossOverlay[32];
    std::snprintf(lossOverlay, sizeof(lossOverlay), "Loss %.1f%%",
                  m_channel.packetLossRate * 100.f);
    ImGui::PlotLines("##loss", m_lossHistory.data(), kHistorySize,
                     m_historyOffset, lossOverlay, 0.f, 100.f, ImVec2(0, 40));

    ImGui::Separator();
    ImGui::Text("RTT         : %.2f ms", m_channel.rttMs);
    ImGui::Text("Jitter      : %.2f ms", m_channel.jitterMs);

    const float lossPercent = m_channel.packetLossRate * 100.f;
    if (lossPercent > 5.f)
        ImGui::TextColored(ImVec4(1, 0.3f, 0.3f, 1), "Packet Loss : %.1f%%", lossPercent);
    else
        ImGui::Text("Packet Loss : %.1f%%", lossPercent);

    ImGui::Text("Sent        : %llu", (unsigned long long)m_channel.packetsSent);
    ImGui::Text("Received    : %llu", (unsigned long long)m_channel.packetsReceived);
    ImGui::Text("Lost        : %llu", (unsigned long long)m_channel.packetsLost);
    ImGui::Text("Retransmit  : %llu", (unsigned long long)m_channel.packetsRetransmit);

    // ─── Replication section ──────────────────────────────────────────────────

    ImGui::SeparatorText("Replication");

    ImGui::Text("Clients         : %u",  m_replication.connectedClients);
    ImGui::Text("Entities rep.   : %u",  m_replication.entitiesReplicated);
    ImGui::Text("Dirty components: %u",  m_replication.dirtyComponents);
    ImGui::Text("Replication Hz  : %.1f", m_replication.replicationHz);

    ImGui::End();
}

} // namespace SagaSandbox