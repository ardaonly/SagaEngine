/// @file ScenarioPickerPanel.cpp

#include "SagaSandbox/UI/Panels/ScenarioPickerPanel.h"
#include "SagaSandbox/Core/ScenarioManager.h"
#include <imgui.h>
#include <algorithm>
#include <map>
#include <string>
#include <vector>

namespace SagaSandbox
{

ScenarioPickerPanel::ScenarioPickerPanel(ScenarioManager& mgr)
    : m_mgr(mgr)
{}

void ScenarioPickerPanel::Render(float /*dt*/, uint64_t /*tick*/)
{
    if (!ImGui::Begin(GetTitle().data(), &m_open))
    {
        ImGui::End();
        return;
    }

    // ─── Active scenario status ───────────────────────────────────────────────

    const std::string activeId = m_mgr.GetActiveScenarioId();

    if (m_mgr.HasActiveScenario())
    {
        ImGui::TextColored(ImVec4(0.2f, 1.f, 0.4f, 1.f),
                           "Active: %s", activeId.c_str());

        if (m_mgr.IsSwitchPending())
        {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(1.f, 0.8f, 0.f, 1.f), "(switch pending)");
        }

        if (ImGui::Button("Stop"))
            m_mgr.RequestStop();
    }
    else
    {
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.f), "No active scenario");
    }

    ImGui::Separator();

    // ─── Group descriptors by category ───────────────────────────────────────

    auto descs = m_mgr.GetAllDescriptors();

    // Sort by category then display name
    std::sort(descs.begin(), descs.end(),
        [](const ScenarioDescriptor* a, const ScenarioDescriptor* b)
        {
            const int cat = a->category.compare(b->category);
            return cat != 0 ? cat < 0 : a->displayName < b->displayName;
        });

    // ─── Render per-category tree nodes ──────────────────────────────────────

    std::string lastCategory;
    bool headerOpen = false;

    for (const ScenarioDescriptor* d : descs)
    {
        const std::string cat{d->category};

        if (cat != lastCategory)
        {
            if (headerOpen)
                ImGui::TreePop();

            headerOpen = ImGui::TreeNodeEx(cat.c_str(),
                             ImGuiTreeNodeFlags_DefaultOpen);
            lastCategory = cat;
        }

        if (!headerOpen) continue;

        const bool isActive = (std::string_view{d->id} == activeId);

        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Leaf |
                                   ImGuiTreeNodeFlags_NoTreePushOnOpen |
                                   ImGuiTreeNodeFlags_SpanFullWidth;

        if (isActive)
            flags |= ImGuiTreeNodeFlags_Selected;

        ImGui::TreeNodeEx(d->id.data(), flags, "%s",
                          d->displayName.data());

        if (ImGui::IsItemClicked() && !isActive)
        {
            m_mgr.RequestSwitch(d->id);
        }

        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("%s", d->description.data());
    }

    if (headerOpen)
        ImGui::TreePop();

    ImGui::End();
}

} // namespace SagaSandbox