/// @file ScenarioPickerPanel.h
/// @brief HUD panel listing all registered scenarios with one-click switch.
///
/// Layer  : Sandbox / UI / Panels
/// Purpose: Reads ScenarioManager::GetAllDescriptors() and renders a
///          category-grouped list. Clicking a row calls
///          ScenarioManager::RequestSwitch().

#pragma once

#include "IDebugPanel.h"

namespace SagaSandbox
{

class ScenarioManager;

class ScenarioPickerPanel final : public IDebugPanel
{
public:
    explicit ScenarioPickerPanel(ScenarioManager& mgr);
    ~ScenarioPickerPanel() override = default;

    [[nodiscard]] std::string_view GetTitle() const noexcept override
    {
        return "Scenarios";
    }

    void Render(float dt, uint64_t tick) override;

private:
    ScenarioManager& m_mgr;
};

} // namespace SagaSandbox