/// @file IDebugPanel.h
/// @brief Interface for a single ImGui debug panel in the sandbox HUD.
///
/// Layer  : Sandbox / UI / Panels
/// Purpose: Each debug panel (FrameStats, Memory, Network…) is an IDebugPanel.
///          DebugHud owns a list of panels and calls Render() per frame.

#pragma once

#include <string_view>
#include <cstdint>

namespace SagaSandbox
{

class IDebugPanel
{
public:
    virtual ~IDebugPanel() = default;

    IDebugPanel(const IDebugPanel&)            = delete;
    IDebugPanel& operator=(const IDebugPanel&) = delete;

    /// Unique window title shown in ImGui.
    [[nodiscard]] virtual std::string_view GetTitle() const noexcept = 0;

    /// Called once per frame inside an ImGui window with GetTitle().
    virtual void Render(float dt, uint64_t tick) = 0;

    /// Whether this panel's ImGui window is open (checked by close button).
    [[nodiscard]] virtual bool IsOpen() const noexcept { return m_open; }
    void Close() noexcept { m_open = false; }

protected:
    IDebugPanel() = default;
    bool m_open = true;
};

} // namespace SagaSandbox