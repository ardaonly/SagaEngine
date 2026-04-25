/// @file IUIFactory.h
/// @brief Abstract factory that creates the concrete IUIApplication and IUIMainWindow.

#pragma once

#include <memory>
#include <string>

namespace SagaEditor
{

class IUIApplication;
class IUIMainWindow;

// ─── UI Factory Interface ─────────────────────────────────────────────────────

/// One concrete implementation exists per supported UI backend (Qt, …).
/// EditorApp selects a factory at startup and uses only this interface
/// afterwards — no backend type ever appears in engine headers.
class IUIFactory
{
public:
    virtual ~IUIFactory() = default;

    /// Create the application object. Must be called before any window is created.
    /// argc/argv are forwarded verbatim to the underlying framework.
    [[nodiscard]] virtual std::unique_ptr<IUIApplication>
        CreateApplication(int& argc, char** argv) = 0;

    /// Create the main window. The application must already exist.
    [[nodiscard]] virtual std::unique_ptr<IUIMainWindow>
        CreateMainWindow(const std::string& title, int width, int height) = 0;
};

} // namespace SagaEditor
