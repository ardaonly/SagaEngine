/// @file ThemeRegistry.cpp
/// @brief Registry and runtime applicator for editor themes.
///
/// Qt (QApplication, QFont) is used here to push the theme to the UI layer,
/// but stays fully contained inside this .cpp — no Qt in the header.

#include "SagaEditor/Themes/ThemeRegistry.h"
#include <QApplication>
#include <QFont>
#include <algorithm>

namespace SagaEditor
{

// ─── Helpers ──────────────────────────────────────────────────────────────────

static QFont FontDescriptorToQFont(const FontDescriptor& fd)
{
    QFont f(QString::fromStdString(fd.family), fd.pointSize);
    f.setBold(fd.bold);
    f.setItalic(fd.italic);
    return f;
}

// ─── Construction ─────────────────────────────────────────────────────────────

ThemeRegistry::ThemeRegistry() = default;

// ─── Registration ─────────────────────────────────────────────────────────────

void ThemeRegistry::Register(EditorTheme theme)
{
    const std::string key = theme.name;
    m_themes[key] = std::move(theme);
}

void ThemeRegistry::Unregister(const std::string& name)
{
    if (name == m_activeName)
        return; // refuse to remove the active theme
    m_themes.erase(name);
}

void ThemeRegistry::RegisterBuiltinThemes()
{
    // ── Dark (default) ────────────────────────────────────────────────────────
    {
        EditorTheme t;
        t.name        = "Dark";
        t.description = "Deep dark theme — easy on the eyes for long sessions.";
        t.builtin     = true;
        t.stylesheet  =
            "QMainWindow, QDialog, QWidget { background:#1e1e1e; color:#d4d4d4; }"
            "QMenuBar { background:#2d2d2d; color:#d4d4d4; }"
            "QMenuBar::item:selected { background:#3a3a3a; }"
            "QMenu { background:#2d2d2d; color:#d4d4d4; border:1px solid #3a3a3a; }"
            "QMenu::item:selected { background:#0e639c; }"
            "QDockWidget { background:#252526; color:#d4d4d4; }"
            "QDockWidget::title { background:#2d2d2d; padding:4px; }"
            "QToolBar { background:#2d2d2d; border:none; }"
            "QStatusBar { background:#007acc; color:#ffffff; }"
            "QTreeView, QListView { background:#1e1e1e; color:#d4d4d4; border:none; }"
            "QTreeView::item:selected { background:#0e639c; }"
            "QLineEdit, QTextEdit { background:#3c3c3c; color:#d4d4d4; border:1px solid #555; }"
            "QPushButton { background:#3a3a3a; color:#d4d4d4; border:1px solid #555; padding:4px 12px; }"
            "QPushButton:hover { background:#4a4a4a; }"
            "QSplitter::handle { background:#3a3a3a; }";
        Register(std::move(t));
    }

    // ── Light ─────────────────────────────────────────────────────────────────
    {
        EditorTheme t;
        t.name        = "Light";
        t.description = "Clean light theme for bright environments.";
        t.builtin     = true;
        t.stylesheet  =
            "QMainWindow, QWidget { background:#f3f3f3; color:#1e1e1e; }"
            "QMenuBar { background:#e8e8e8; color:#1e1e1e; }"
            "QDockWidget::title { background:#dcdcdc; padding:4px; }"
            "QToolBar { background:#e8e8e8; border:none; }"
            "QStatusBar { background:#0078d4; color:#ffffff; }";
        Register(std::move(t));
    }

    // ── Nord ──────────────────────────────────────────────────────────────────
    {
        EditorTheme t;
        t.name        = "Nord";
        t.description = "Arctic nord color palette — cool and calming.";
        t.builtin     = true;
        t.stylesheet  =
            "QMainWindow, QWidget { background:#2e3440; color:#d8dee9; }"
            "QMenuBar { background:#3b4252; color:#d8dee9; }"
            "QDockWidget::title { background:#3b4252; padding:4px; }"
            "QToolBar { background:#3b4252; border:none; }"
            "QStatusBar { background:#5e81ac; color:#eceff4; }"
            "QTreeView::item:selected { background:#4c566a; }";
        Register(std::move(t));
    }

    // ── Solarized Dark ────────────────────────────────────────────────────────
    {
        EditorTheme t;
        t.name        = "SolarizedDark";
        t.description = "Ethan Schoonover's Solarized dark palette.";
        t.builtin     = true;
        t.stylesheet  =
            "QMainWindow, QWidget { background:#002b36; color:#839496; }"
            "QMenuBar { background:#073642; color:#839496; }"
            "QDockWidget::title { background:#073642; padding:4px; }"
            "QToolBar { background:#073642; border:none; }"
            "QStatusBar { background:#268bd2; color:#fdf6e3; }";
        Register(std::move(t));
    }

    // ── Midnight ──────────────────────────────────────────────────────────────
    {
        EditorTheme t;
        t.name        = "Midnight";
        t.description = "Pure black theme for OLED displays and night sessions.";
        t.builtin     = true;
        t.stylesheet  =
            "QMainWindow, QWidget { background:#000000; color:#e0e0e0; }"
            "QMenuBar { background:#0a0a0a; color:#e0e0e0; }"
            "QDockWidget::title { background:#0a0a0a; padding:4px; }"
            "QToolBar { background:#0a0a0a; border:none; }"
            "QStatusBar { background:#1a1a2e; color:#e0e0e0; }"
            "QTreeView::item:selected { background:#16213e; }";
        Register(std::move(t));
    }

    Apply("Dark");
}

// ─── Activation ───────────────────────────────────────────────────────────────

bool ThemeRegistry::Apply(const std::string& name)
{
    auto it = m_themes.find(name);
    if (it == m_themes.end())
        return false;

    const EditorTheme& theme = it->second;

    // Push stylesheet and font to the Qt application object.
    if (qApp)
    {
        qApp->setStyleSheet(QString::fromStdString(theme.stylesheet));
        if (!theme.uiFont.family.empty())
            qApp->setFont(FontDescriptorToQFont(theme.uiFont));
    }

    m_activeName = name;

    for (auto& cb : m_callbacks)
        cb(theme);

    return true;
}

const EditorTheme& ThemeRegistry::GetActive() const noexcept
{
    auto it = m_themes.find(m_activeName);
    static const EditorTheme kFallback{};
    return (it != m_themes.end()) ? it->second : kFallback;
}

const std::string& ThemeRegistry::GetActiveName() const noexcept
{
    return m_activeName;
}

// ─── Queries ──────────────────────────────────────────────────────────────────

const EditorTheme* ThemeRegistry::Find(const std::string& name) const
{
    auto it = m_themes.find(name);
    return (it != m_themes.end()) ? &it->second : nullptr;
}

std::vector<std::string> ThemeRegistry::GetAllNames() const
{
    std::vector<std::string> names;
    names.reserve(m_themes.size());
    for (const auto& [key, _] : m_themes)
        names.push_back(key);
    std::sort(names.begin(), names.end());
    return names;
}

void ThemeRegistry::OnThemeChanged(ThemeChangedCallback cb)
{
    m_callbacks.push_back(std::move(cb));
}

} // namespace SagaEditor
