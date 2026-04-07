/// @file Application.h
/// @brief Core engine application — owns the main loop, window, and subsystem lifecycle.

#pragma once

#include "SagaEngine/Platform/IWindow.h"
#include <memory>
#include <string>

namespace Saga {

class IWindow;

// ─── Application ──────────────────────────────────────────────────────────────

/// Root engine object; exactly one instance exists per process.
///
/// Lifecycle:
///   Construction → Run() → [OnInit → game loop → OnShutdown] → destruction.
///
/// Subclassing contract:
///   Concrete applications (SandboxApp, ClientApp, ServerApp) override
///   OnInit / OnUpdate / OnShutdown to install their own subsystems.
///   They must not call Run() themselves — the entry point calls it.
///
/// Window ownership:
///   Application owns the IWindow produced by PlatformFactory::CreateWindow().
///   Subsystems that need the window (e.g. RHI) receive a non-owning pointer.
class Application
{
public:
    /// Construct the application with the given display name.
    explicit Application(std::string name);
    virtual ~Application() = default;

    Application(const Application&)            = delete;
    Application& operator=(const Application&) = delete;

    // ─── Entry Point ──────────────────────────────────────────────────────────

    /// Start the engine: create the window, call OnInit, run the frame loop,
    /// call OnShutdown, and release all resources.
    /// Blocks until the window is closed or ShouldClose() is set.
    void Run();

    // ─── Accessors ────────────────────────────────────────────────────────────

    /// The human-readable name passed at construction (used as window title).
    [[nodiscard]] const std::string& GetName() const noexcept { return m_Name; }

    /// Non-owning pointer to the active window; valid between OnInit and OnShutdown.
    /// Returns nullptr outside that range.
    [[nodiscard]] IWindow* GetWindow() const noexcept { return m_Window.get(); }

    /// Signal the main loop to exit cleanly after the current frame.
    void RequestClose() noexcept;

protected:
    // ─── Overridable Hooks ────────────────────────────────────────────────────

    /// Called once after the window is open and before the frame loop begins.
    /// Register subsystems and load startup assets here.
    virtual void OnInit() {}

    /// Called once per frame after PollEvents and before Present.
    virtual void OnUpdate() {}

    /// Called once after the frame loop exits, before the window is destroyed.
    /// Shutdown order should be the reverse of OnInit registration order.
    virtual void OnShutdown() {}

private:
    bool m_ShouldClose = false;
    std::string              m_Name;   ///< Display name; also used as the window title.
    std::unique_ptr<IWindow> m_Window; ///< Owned platform window; null until Run() is called.
};

} // namespace Saga