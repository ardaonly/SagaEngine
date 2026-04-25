/// @file ThemeRegistry.h
/// @brief Registry and runtime applicator for editor themes.

#pragma once

#include "SagaEditor/Themes/EditorTheme.h"
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace SagaEditor
{

// ─── Theme Registry ───────────────────────────────────────────────────────────

/// Stores all available EditorThemes and applies the active one to the UI backend.
/// Themes can be registered by packages at any point before the editor renders
/// its first frame. Switching themes at runtime takes effect immediately.
class ThemeRegistry
{
public:
    using ThemeChangedCallback = std::function<void(const EditorTheme&)>;

    ThemeRegistry();

    // ─── Registration ─────────────────────────────────────────────────────────

    /// Add a theme to the registry. Overwrites any existing theme with the same name.
    void Register(EditorTheme theme);

    /// Remove a theme by name. No-op if the theme is currently active.
    void Unregister(const std::string& name);

    /// Register the five built-in themes (Dark, Light, Nord, SolarizedDark, Midnight).
    void RegisterBuiltinThemes();

    // ─── Activation ───────────────────────────────────────────────────────────

    /// Apply a theme by name. Returns false if the name is not registered.
    [[nodiscard]] bool Apply(const std::string& name);

    /// Return the currently active theme.
    [[nodiscard]] const EditorTheme& GetActive() const noexcept;

    /// Return the name of the currently active theme.
    [[nodiscard]] const std::string& GetActiveName() const noexcept;

    // ─── Queries ──────────────────────────────────────────────────────────────

    [[nodiscard]] const EditorTheme*       Find(const std::string& name) const;
    [[nodiscard]] std::vector<std::string> GetAllNames()                 const;

    // ─── Change Notification ──────────────────────────────────────────────────

    /// Register a callback fired every time the active theme changes.
    void OnThemeChanged(ThemeChangedCallback cb);

private:
    std::unordered_map<std::string, EditorTheme> m_themes;
    std::string                                   m_activeName;
    std::vector<ThemeChangedCallback>             m_callbacks;
};

} // namespace SagaEditor
