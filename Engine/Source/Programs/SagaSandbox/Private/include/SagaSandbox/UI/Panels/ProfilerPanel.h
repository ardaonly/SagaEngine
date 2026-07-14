/// @file ProfilerPanel.h
/// @brief HUD panel displaying Profiler sample table.
///
/// Layer  : Sandbox / UI / Panels
/// Purpose: Calls Profiler::GetAllSamplesSnapshot() each frame and renders
///          a sortable table of call counts, total time, and avg/min/max.
///
/// NOTE: Requires the Profiler patch (GetAllSamplesSnapshot public method).
///       See PROFILER_PATCH_NOTE.txt.

#pragma once

#include "IDebugPanel.h"
#include <SagaEngine/Core/Profiling/Profiler.h>
#include <string>
#include <vector>

namespace SagaSandbox
{

class ProfilerPanel final : public IDebugPanel
{
public:
    ProfilerPanel()  = default;
    ~ProfilerPanel() override = default;

    [[nodiscard]] std::string_view GetTitle() const noexcept override
    {
        return "Profiler";
    }

    void Render(float dt, uint64_t tick) override;

private:
    // ─── Sort state ───────────────────────────────────────────────────────────
    int  m_sortColumn    = 2;   ///< 0=name, 1=calls, 2=total, 3=avg, 4=min, 5=max
    bool m_sortAscending = false;

    /// Cached snapshot so we don't re-lock the profiler mutex on every ImGui row
    struct Row
    {
        std::string name;
        uint64_t    calls    = 0;
        double      totalMs  = 0.0;
        double      avgUs    = 0.0;
        double      minUs    = 0.0;
        double      maxUs    = 0.0;
    };

    std::vector<Row> m_rows;
    float            m_refreshAccum = 0.f;
    static constexpr float kRefreshInterval = 0.25f;  ///< Refresh every 250ms
};

} // namespace SagaSandbox