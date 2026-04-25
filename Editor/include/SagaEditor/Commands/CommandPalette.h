/// @file CommandPalette.h
/// @brief Searchable command palette — Ctrl+Shift+P to open.

#pragma once

#include <memory>
#include <string>

namespace SagaEditor
{

class CommandRegistry;
class CommandDispatcher;

// ─── Command Palette ──────────────────────────────────────────────────────────

/// Service for quick-access command filtering and invocation.
/// Shows a searchable list; type to filter, Enter to invoke, Escape to dismiss.
/// The underlying Qt dialog is hidden in the implementation (pimpl pattern).
class CommandPalette
{
public:
    CommandPalette(CommandRegistry&   registry,
                   CommandDispatcher& dispatcher);
    ~CommandPalette();

    /// Show the palette, pre-focused and ready for input.
    void Show();

    /// Hide the palette.
    void Hide();

    /// Query whether the palette is currently visible.
    [[nodiscard]] bool IsVisible() const;

private:
    class Impl; ///< Qt dialog and UI live here.
    std::unique_ptr<Impl> m_impl; ///< Pimpl to hide Qt dependencies.
};

} // namespace SagaEditor
