/// @file EditorProfileActivator.h
/// @brief Fans active editor profiles out to shell-level workflow sinks.

#pragma once

#include "SagaEditor/Profile/EditorProfile.h"

#include <functional>
#include <string>
#include <vector>

namespace SagaEditor
{

class EditorProfileRegistry;

struct EditorProfileApplyResult
{
    bool layoutApplied = false;
    bool shortcutMapApplied = false;
    bool toolbarApplied = false;
    bool panelVisibilityApplied = false;
    bool shellChromeApplied = false;
    bool toolVisibilityApplied = false;
    std::string lastError;

    [[nodiscard]] bool AllApplied() const noexcept
    {
        return layoutApplied && shortcutMapApplied && toolbarApplied &&
               panelVisibilityApplied && shellChromeApplied && toolVisibilityApplied;
    }

    [[nodiscard]] bool AnyApplied() const noexcept
    {
        return layoutApplied || shortcutMapApplied || toolbarApplied ||
               panelVisibilityApplied || shellChromeApplied || toolVisibilityApplied;
    }
};

class EditorProfileActivator
{
public:
    using LayoutSink = std::function<bool(const std::string& layoutPresetId)>;
    using ShortcutMapSink = std::function<bool(const EditorProfile& profile)>;
    using ToolbarSink = std::function<bool(const std::vector<std::string>& commands)>;
    using PanelVisibilitySink = std::function<bool(const std::vector<std::string>& panels)>;
    using ShellChromeSink = std::function<bool(const EditorProfile& profile)>;
    using ToolVisibilitySink = std::function<bool(const std::vector<std::string>& commands)>;

    void SetLayoutSink(LayoutSink sink) noexcept;
    void SetShortcutMapSink(ShortcutMapSink sink) noexcept;
    void SetToolbarSink(ToolbarSink sink) noexcept;
    void SetPanelVisibilitySink(PanelVisibilitySink sink) noexcept;
    void SetShellChromeSink(ShellChromeSink sink) noexcept;
    void SetToolVisibilitySink(ToolVisibilitySink sink) noexcept;

    void ClearSinks() noexcept;

    [[nodiscard]] EditorProfileApplyResult Apply(const EditorProfile& profile);
    [[nodiscard]] EditorProfileApplyResult ApplyActive(const EditorProfileRegistry& registry);

private:
    LayoutSink m_layoutSink;
    ShortcutMapSink m_shortcutMapSink;
    ToolbarSink m_toolbarSink;
    PanelVisibilitySink m_panelVisibilitySink;
    ShellChromeSink m_shellChromeSink;
    ToolVisibilitySink m_toolVisibilitySink;
};

} // namespace SagaEditor
