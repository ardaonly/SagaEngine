/// @file MemoryStatsPanel.h
/// @brief HUD panel feeding MemoryTracker data into the sandbox HUD.
///
/// Layer  : Sandbox / UI / Panels
/// Purpose: Reads SagaEngine::Core::MemoryTracker::Instance() each frame
///          and displays heap totals, allocation count, and peak usage.

#pragma once

#include "IDebugPanel.h"
#include <array>
#include <cstddef>

namespace SagaSandbox
{

class MemoryStatsPanel final : public IDebugPanel
{
public:
    MemoryStatsPanel()  = default;
    ~MemoryStatsPanel() override = default;

    [[nodiscard]] std::string_view GetTitle() const noexcept override
    {
        return "Memory";
    }

    void Render(float dt, uint64_t tick) override;

private:
    static constexpr int kHistorySize = 128;
    std::array<float, kHistorySize> m_allocHistory{};  ///< MB history for plot
    int    m_historyOffset = 0;
    size_t m_peakAllocated = 0;
};

} // namespace SagaSandbox