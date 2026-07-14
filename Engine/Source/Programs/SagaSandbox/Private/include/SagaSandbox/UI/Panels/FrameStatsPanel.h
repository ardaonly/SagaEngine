/// @file FrameStatsPanel.h
/// @brief HUD panel displaying frame time, tick rate, and simulation stats.
///
/// Layer  : Sandbox / UI / Panels
/// Purpose: Always-visible panel showing the core observability metrics:
///          frame time, tick count, entity count, memory usage.

#pragma once

#include "IDebugPanel.h"
#include <array>

namespace SagaSandbox
{

class FrameStatsPanel final : public IDebugPanel
{
public:
    FrameStatsPanel()  = default;
    ~FrameStatsPanel() override = default;

    [[nodiscard]] std::string_view GetTitle() const noexcept override
    {
        return "Frame Stats";
    }

    void Render(float dt, uint64_t tick) override;

private:
    // Ring buffer of recent frame times for the sparkline graph
    static constexpr int kHistorySize = 128;
    std::array<float, kHistorySize> m_frameTimes{};
    int                             m_historyOffset = 0;
    float                           m_worstFrameMs  = 0.f;
};

} // namespace SagaSandbox