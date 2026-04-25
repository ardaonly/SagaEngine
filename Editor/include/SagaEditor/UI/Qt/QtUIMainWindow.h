/// @file QtUIMainWindow.h
/// @brief Qt backend implementation of IUIMainWindow — wraps QMainWindow.

#pragma once

#include "SagaEditor/UI/IUIMainWindow.h"
#include <memory>

namespace SagaEditor
{

// ─── Qt Main Window ───────────────────────────────────────────────────────────

/// Concrete QMainWindow hidden behind IUIMainWindow.
/// All Qt types (QMainWindow, QDockWidget, QAction, …) live in the .cpp;
/// this header stays framework-free.
class QtUIMainWindow final : public IUIMainWindow
{
public:
    QtUIMainWindow(const std::string& title, int width, int height);
    ~QtUIMainWindow() override;

    // ─── IUIMainWindow ────────────────────────────────────────────────────────

    void Show()                              override;
    void ShowMaximized()                     override;
    void Hide()                              override;
    void Close()                             override;
    void SetTitle(const std::string& title)  override;
    void SetSize(int width, int height)      override;

    [[nodiscard]] int   GetWidth()           const noexcept override;
    [[nodiscard]] int   GetHeight()          const noexcept override;
    [[nodiscard]] void* GetNativeHandle()    const noexcept override;

    void ApplyShellLayout(const ShellLayout& layout)  override;
    void SetStatusMessage(const std::string& message) override;

    void DockPanel(void*              nativeWidget,
                   const std::string& panelId,
                   const std::string& title,
                   UIDockArea         area)            override;
    void UndockPanel(const std::string& panelId)      override;
    void FocusPanel (const std::string& panelId)      override;

    [[nodiscard]] std::vector<uint8_t> SaveState()                             const override;
    [[nodiscard]] bool                 RestoreState(const std::vector<uint8_t>& state) override;

    void SetOnClose(CloseCallback cb) override;

private:
    struct Impl;                   ///< Pimpl — hides QMainWindow and all Qt types.
    std::unique_ptr<Impl> m_impl;
};

} // namespace SagaEditor
