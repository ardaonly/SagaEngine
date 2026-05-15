#pragma once

/// @file IPlatformInputBackend.h
/// @brief Pure interface for platform input backends (SDL, Win32, XInput...).
///
/// Layer  : Platform
/// Purpose: Sole point of contact between the OS/SDK and the input system.
///          Implementations: SDLInputBackend, Win32InputBackend, etc.
///
///
/// Why the old IInputDevice was wrong:
///   - IsKeyDown() leaks state tracking into the backend layer.
///     State tracking belongs in InputState (Input::Core).
///   - PollEvent() returned one event at a time, causing interleaving
///     and making consistent per-frame snapshots impossible.
///   - PollTextInput() as a separate string-based call had no timestamp
///     and didn't compose with the frame model.
///
/// This interface fixes all of the above:
///   - Backend produces a RawInputFrame per poll — a complete snapshot.
///   - State tracking is zero — backend collects events only.
///   - Text input flows through the same frame model.
///   - The interface is small and stable; backends implement exactly this.
///
/// Thread safety:
///   PollRawEvents() must be called from a single thread (main/input thread).
///   The backend itself is not thread-safe. The caller is responsible
///   for synchronizing access if the frame is passed to other threads.

#include "SagaEngine/Input/Frames/RawInputFrame.h"
#include <string_view>
#include <cstdint>

namespace SagaEngine::Platform
{

class IPlatformInputBackend
{
public:
    virtual ~IPlatformInputBackend() = default;

    // Lifecycle

    /// Initialize backend resources. Returns false on failure.
    /// Must be called once before PollRawEvents().
    virtual bool Initialize() = 0;

    /// Release all backend resources.
    virtual void Shutdown() = 0;

    // Core contract

    /// Collect all pending OS events and append them to outFrame.
    /// outFrame is cleared by this call before filling.
    /// Must be called once per frame from the input thread.
    virtual void PollRawEvents(Input::RawInputFrame& outFrame) = 0;

    // Text input

    /// Enable or disable OS text input mode.
    /// When enabled: text events flow through RawInputFrame::text.
    /// When enabled: OS soft keyboard / IME may appear.
    /// Game input (WASD, etc.) should work regardless of this flag.
    virtual void SetTextInputEnabled(bool enabled) = 0;

    // Cursor control

    /// Warp cursor to absolute window coordinates.
    /// Default: no-op. Override for mouse-look / cursor lock.
    virtual void WarpCursor(int32_t x, int32_t y) { (void)x; (void)y; }

    /// Show or hide the OS cursor.
    virtual void SetCursorVisible(bool visible) { (void)visible; }

    /// Lock cursor to window center (raw mouse input mode for look cameras).
    /// When locked: mouseMoves report only relX/relY, absX/absY may be 0.
    virtual void SetCursorLocked(bool locked) { (void)locked; }

    // Diagnostics

    [[nodiscard]] virtual std::string_view GetBackendName() const noexcept = 0;

    // Non-copyable, non-movable (interface contracts own resources)
    IPlatformInputBackend(const IPlatformInputBackend&) = delete;
    IPlatformInputBackend& operator=(const IPlatformInputBackend&) = delete;

protected:
    IPlatformInputBackend() = default;
};

} // namespace SagaEngine::Platform