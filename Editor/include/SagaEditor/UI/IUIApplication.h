/// @file IUIApplication.h
/// @brief Abstract UI application — owns the event loop lifecycle.

#pragma once

namespace SagaEditor
{

// ─── Application Interface ────────────────────────────────────────────────────

/// Wraps the framework's main application object (e.g. QApplication).
/// The editor core calls Run() and Quit() through this interface; no framework
/// type ever appears in EditorApp.h or any other engine header.
class IUIApplication
{
public:
    virtual ~IUIApplication() = default;

    /// Enter the framework event loop. Blocks until Quit() is called.
    /// Returns the framework exit code.
    virtual int Run() = 0;

    /// Request a clean exit from the event loop.
    virtual void Quit() = 0;
};

} // namespace SagaEditor
