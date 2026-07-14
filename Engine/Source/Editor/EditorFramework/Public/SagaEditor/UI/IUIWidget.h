/// @file IUIWidget.h
/// @brief Abstract UI widget — the atom of the UI abstraction layer.

#pragma once

#include <cstdint>
#include <string>

namespace SagaEditor
{

// ─── Widget Interface ─────────────────────────────────────────────────────────

/// Wraps any native widget without exposing the UI framework type in
/// engine headers. Callers that need the real framework object cast
/// GetNativeHandle() at the backend boundary; all other code stays framework-free.
class IUIWidget
{
public:
    virtual ~IUIWidget() = default;

    /// Return the underlying framework widget pointer (e.g. QWidget* on Qt).
    /// Callers outside the Qt backend must not dereference this directly.
    [[nodiscard]] virtual void* GetNativeHandle() const noexcept = 0;

    virtual void Show()    = 0;
    virtual void Hide()    = 0;
    virtual void SetEnabled(bool enabled) = 0;

    [[nodiscard]] virtual bool IsVisible()  const noexcept = 0;
    [[nodiscard]] virtual bool IsEnabled()  const noexcept = 0;

    [[nodiscard]] virtual int  GetWidth()   const noexcept = 0;
    [[nodiscard]] virtual int  GetHeight()  const noexcept = 0;
};

} // namespace SagaEditor
