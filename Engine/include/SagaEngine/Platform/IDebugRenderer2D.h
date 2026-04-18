/// @file IDebugRenderer2D.h
/// @brief Platform-agnostic 2D debug renderer for diagnostic overlays.
///
/// Layer  : SagaEngine / Platform
/// Purpose: Lightweight software renderer for drawing debug primitives
///          (rectangles, lines) without requiring a full GPU backend.
///          Used by the client host to visualise interpolated entity
///          positions before the Diligent render pipeline is wired.
///
/// Ownership: created via IWindow::CreateDebugRenderer2D().

#pragma once

#include <cstdint>
#include <memory>

namespace Saga {

class IWindow;

// ─── IDebugRenderer2D ────────────────────────────────────────────────────────

/// Thin interface for platform-specific 2D debug rendering.
/// Implementations live alongside their window backend (e.g. SDL).
class IDebugRenderer2D
{
public:
    virtual ~IDebugRenderer2D() = default;

    /// Initialise the renderer attached to the given window.
    /// @param window   The window to render into.
    /// @param vsync    Enable vertical sync.
    /// @return true on success.
    virtual bool Init(IWindow& window, bool vsync = false) = 0;

    /// Shut down and release platform resources.
    virtual void Shutdown() = 0;

    // ── Drawing ──────────────────────────────────────────────────────────────

    /// Set the active draw colour (RGBA, 0–255).
    virtual void SetColor(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) = 0;

    /// Clear the back buffer with the current colour.
    virtual void Clear() = 0;

    /// Draw a filled axis-aligned rectangle.
    virtual void FillRect(int x, int y, int w, int h) = 0;

    /// Present the rendered frame.
    virtual void Present() = 0;
};

} // namespace Saga
