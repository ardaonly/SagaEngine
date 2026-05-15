/// @file EditorProfileActivator.cpp
/// @brief Editor profile activation fan-out implementation.

#include "SagaEditor/Profile/EditorProfileActivator.h"

#include "SagaEditor/Profile/EditorProfileRegistry.h"

#include <utility>

namespace SagaEditor
{

void EditorProfileActivator::SetLayoutSink(LayoutSink sink) noexcept
{
    m_layoutSink = std::move(sink);
}

void EditorProfileActivator::SetShortcutMapSink(ShortcutMapSink sink) noexcept
{
    m_shortcutMapSink = std::move(sink);
}

void EditorProfileActivator::SetToolbarSink(ToolbarSink sink) noexcept
{
    m_toolbarSink = std::move(sink);
}

void EditorProfileActivator::SetPanelVisibilitySink(PanelVisibilitySink sink) noexcept
{
    m_panelVisibilitySink = std::move(sink);
}

void EditorProfileActivator::SetShellChromeSink(ShellChromeSink sink) noexcept
{
    m_shellChromeSink = std::move(sink);
}

void EditorProfileActivator::SetToolVisibilitySink(ToolVisibilitySink sink) noexcept
{
    m_toolVisibilitySink = std::move(sink);
}

void EditorProfileActivator::ClearSinks() noexcept
{
    m_layoutSink = nullptr;
    m_shortcutMapSink = nullptr;
    m_toolbarSink = nullptr;
    m_panelVisibilitySink = nullptr;
    m_shellChromeSink = nullptr;
    m_toolVisibilitySink = nullptr;
}

namespace
{

template <typename Sink, typename Arg>
bool RunSink(const Sink& sink,
             const Arg& arg,
             bool& outRanFlag,
             std::string& outError,
             const char* sinkName)
{
    if (!sink)
    {
        return true;
    }
    const bool ok = sink(arg);
    if (!ok)
    {
        outError.assign(sinkName);
        outError.append(" sink reported failure");
        return false;
    }
    outRanFlag = true;
    return true;
}

} // namespace

EditorProfileApplyResult EditorProfileActivator::Apply(const EditorProfile& profile)
{
    EditorProfileApplyResult result;

    RunSink(m_layoutSink, profile.layoutPresetId,
            result.layoutApplied, result.lastError, "layout");
    RunSink(m_shortcutMapSink, profile,
            result.shortcutMapApplied, result.lastError, "shortcutMap");
    RunSink(m_toolbarSink, profile.defaultToolbarCommands,
            result.toolbarApplied, result.lastError, "toolbar");
    RunSink(m_panelVisibilitySink, profile.defaultPanels,
            result.panelVisibilityApplied, result.lastError, "panelVisibility");
    RunSink(m_shellChromeSink, profile,
            result.shellChromeApplied, result.lastError, "shellChrome");
    RunSink(m_toolVisibilitySink, profile.visibleToolCommands,
            result.toolVisibilityApplied, result.lastError, "toolVisibility");

    return result;
}

EditorProfileApplyResult
EditorProfileActivator::ApplyActive(const EditorProfileRegistry& registry)
{
    const EditorProfile* active = registry.GetActive();
    if (active == nullptr)
    {
        EditorProfileApplyResult result;
        result.lastError = "no active editor profile";
        return result;
    }
    return Apply(*active);
}

} // namespace SagaEditor
