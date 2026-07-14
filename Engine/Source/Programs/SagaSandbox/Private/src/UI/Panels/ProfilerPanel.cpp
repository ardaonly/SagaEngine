/// @file ProfilerPanel.cpp
/// @brief Sandbox HUD profiler panel — sortable sample table.

#include "SagaSandbox/UI/Panels/ProfilerPanel.h"
#include <imgui.h>
#include <algorithm>
#include <cstdio>

namespace SagaSandbox
{

void ProfilerPanel::Render(float dt, uint64_t /*tick*/)
{
    if (!ImGui::Begin(GetTitle().data(), &m_open))
    {
        ImGui::End();
        return;
    }

    // ─── Refresh snapshot at interval ────────────────────────────────────────

    m_refreshAccum += dt;
    if (m_refreshAccum >= kRefreshInterval || m_rows.empty())
    {
        m_refreshAccum = 0.f;
        m_rows.clear();

        // GetAllSamplesSnapshot() — requires the Profiler patch
        auto raw = SagaEngine::Core::Profiler::Instance().GetAllSamplesSnapshot();
        m_rows.reserve(raw.size());

        for (const auto& [name, stats] : raw)
        {
            const uint64_t calls    = stats.callCount.load(std::memory_order_relaxed);
            const uint64_t totalNs  = stats.totalTimeNs.load(std::memory_order_relaxed);
            const uint64_t minNs    = stats.minTimeNs.load(std::memory_order_relaxed);
            const uint64_t maxNs    = stats.maxTimeNs.load(std::memory_order_relaxed);

            if (calls == 0) continue;

            Row r;
            r.name    = name;
            r.calls   = calls;
            r.totalMs = static_cast<double>(totalNs) / 1'000'000.0;
            r.avgUs   = static_cast<double>(totalNs) / static_cast<double>(calls) / 1000.0;
            r.minUs   = static_cast<double>(minNs) / 1000.0;
            r.maxUs   = static_cast<double>(maxNs) / 1000.0;
            m_rows.push_back(std::move(r));
        }
    }

    // ─── Sort ─────────────────────────────────────────────────────────────────

    std::sort(m_rows.begin(), m_rows.end(), [this](const Row& a, const Row& b)
    {
        bool less = false;
        switch (m_sortColumn)
        {
        case 0: less = a.name    < b.name;    break;
        case 1: less = a.calls   < b.calls;   break;
        case 2: less = a.totalMs < b.totalMs; break;
        case 3: less = a.avgUs   < b.avgUs;   break;
        case 4: less = a.minUs   < b.minUs;   break;
        case 5: less = a.maxUs   < b.maxUs;   break;
        default: break;
        }
        return m_sortAscending ? less : !less;
    });

    // ─── Controls ─────────────────────────────────────────────────────────────

    ImGui::Text("%zu samples | refresh: %.0fms",
                m_rows.size(), kRefreshInterval * 1000.f);

    if (ImGui::Button("Clear"))
        SagaEngine::Core::Profiler::Instance().Clear();

    ImGui::SameLine();
    if (ImGui::Button("Dump to File"))
        SagaEngine::Core::Profiler::Instance().DumpReport("profiler_dump.txt");

    ImGui::Separator();

    // ─── Table ────────────────────────────────────────────────────────────────

    constexpr ImGuiTableFlags kFlags =
        ImGuiTableFlags_Resizable    |
        ImGuiTableFlags_Sortable     |
        ImGuiTableFlags_RowBg        |
        ImGuiTableFlags_BordersOuter |
        ImGuiTableFlags_ScrollY;

    if (ImGui::BeginTable("##prof", 6, kFlags, ImVec2(0, 0)))
    {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("Sample",    ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Calls",     ImGuiTableColumnFlags_WidthFixed, 70.f);
        ImGui::TableSetupColumn("Total ms",  ImGuiTableColumnFlags_WidthFixed, 80.f);
        ImGui::TableSetupColumn("Avg us",    ImGuiTableColumnFlags_WidthFixed, 70.f);
        ImGui::TableSetupColumn("Min us",    ImGuiTableColumnFlags_WidthFixed, 70.f);
        ImGui::TableSetupColumn("Max us",    ImGuiTableColumnFlags_WidthFixed, 70.f);
        ImGui::TableHeadersRow();

        // Handle click-to-sort on headers
        if (ImGuiTableSortSpecs* sortSpecs = ImGui::TableGetSortSpecs())
        {
            if (sortSpecs->SpecsDirty && sortSpecs->SpecsCount > 0)
            {
                m_sortColumn    = sortSpecs->Specs[0].ColumnIndex;
                m_sortAscending = (sortSpecs->Specs[0].SortDirection == ImGuiSortDirection_Ascending);
                sortSpecs->SpecsDirty = false;
            }
        }

        char buf[64];
        for (const auto& row : m_rows)
        {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted(row.name.c_str());
            ImGui::TableSetColumnIndex(1);
            std::snprintf(buf, sizeof(buf), "%llu", (unsigned long long)row.calls);
            ImGui::TextUnformatted(buf);
            ImGui::TableSetColumnIndex(2);
            std::snprintf(buf, sizeof(buf), "%.3f", row.totalMs);
            ImGui::TextUnformatted(buf);
            ImGui::TableSetColumnIndex(3);
            std::snprintf(buf, sizeof(buf), "%.1f", row.avgUs);
            ImGui::TextUnformatted(buf);
            ImGui::TableSetColumnIndex(4);
            std::snprintf(buf, sizeof(buf), "%.1f", row.minUs);
            ImGui::TextUnformatted(buf);
            ImGui::TableSetColumnIndex(5);
            std::snprintf(buf, sizeof(buf), "%.1f", row.maxUs);
            ImGui::TextUnformatted(buf);
        }

        ImGui::EndTable();
    }

    ImGui::End();
}

} // namespace SagaSandbox