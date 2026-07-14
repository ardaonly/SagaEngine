/// @file WorldViewportPanel.h
/// @brief World viewport panel — 3D scene view with camera controls.
///
/// Qt-free header. All widget internals (paintEvent, mouse events, etc.)
/// live in the Impl (WorldViewportPanel.cpp) — the Qt surface stays fully
/// contained inside the backend.

#pragma once

#include "SagaEditor/Panels/IPanel.h"
#include <cstdint>
#include <memory>
#include <string>

namespace SagaEditor
{

// ─── Viewport Mode ────────────────────────────────────────────────────────────

enum class ViewportMode : uint8_t { Perspective, Top, Front, Right };

// ─── World Viewport Panel ─────────────────────────────────────────────────────

/// Renders the 3D world view.
/// Handles mouse orbit, WASD fly-cam, and scroll-to-zoom.
class WorldViewportPanel : public IPanel
{
public:
    WorldViewportPanel();
    ~WorldViewportPanel() override;

    // ─── IPanel ───────────────────────────────────────────────────────────────
    [[nodiscard]] PanelId     GetPanelId()      const override;
    [[nodiscard]] std::string GetTitle()        const override;
    [[nodiscard]] void*       GetNativeWidget() const noexcept override;

    void OnInit()     override;
    void OnShutdown() override;

    // ─── Camera ───────────────────────────────────────────────────────────────
    void                        SetViewportMode(ViewportMode mode);
    [[nodiscard]] ViewportMode  GetViewportMode() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace SagaEditor
