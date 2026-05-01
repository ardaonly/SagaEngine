/// @file PersonaActivator.cpp
/// @brief Implementation of the persona-to-subsystems fan-out.

#include "SagaEditor/Persona/PersonaActivator.h"

#include "SagaEditor/Persona/PersonaRegistry.h"

#include <utility>

namespace SagaEditor
{

// ─── Sink Installation ────────────────────────────────────────────────────────

void PersonaActivator::SetThemeSink(ThemeSink sink) noexcept
{
    m_themeSink = std::move(sink);
}

void PersonaActivator::SetLayoutSink(LayoutSink sink) noexcept
{
    m_layoutSink = std::move(sink);
}

void PersonaActivator::SetDensitySink(DensitySink sink) noexcept
{
    m_densitySink = std::move(sink);
}

void PersonaActivator::SetBlockStyleSink(BlockStyleSink sink) noexcept
{
    m_blockStyleSink = std::move(sink);
}

void PersonaActivator::SetToolbarSink(ToolbarSink sink) noexcept
{
    m_toolbarSink = std::move(sink);
}

void PersonaActivator::SetPanelVisibilitySink(PanelVisibilitySink sink) noexcept
{
    m_panelVisibilitySink = std::move(sink);
}

void PersonaActivator::ClearSinks() noexcept
{
    m_themeSink           = nullptr;
    m_layoutSink          = nullptr;
    m_densitySink         = nullptr;
    m_blockStyleSink      = nullptr;
    m_toolbarSink         = nullptr;
    m_panelVisibilitySink = nullptr;
}

// ─── Apply Helper ─────────────────────────────────────────────────────────────

namespace
{

/// Run `sink` if installed, capturing success and a failure message.
/// Returns false when the sink ran and reported failure; returns true
/// when the sink ran and succeeded; returns true when the sink is
/// unwired (uninstalled sinks count as "skipped" rather than "failed"
/// — see PersonaApplyResult docstring).
template <typename Sink, typename Arg>
bool RunSink(const Sink& sink,
             const Arg&  arg,
             bool&       outRanFlag,
             std::string& outError,
             const char* sinkName)
{
    if (!sink)
    {
        return true; // not wired yet
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

// ─── Apply ────────────────────────────────────────────────────────────────────

PersonaApplyResult PersonaActivator::Apply(const UIPersona& persona)
{
    PersonaApplyResult result;

    RunSink(m_themeSink,           persona.themeId,
            result.themeApplied,         result.lastError, "theme");
    RunSink(m_layoutSink,          persona.workspacePresetId,
            result.layoutApplied,        result.lastError, "layout");
    RunSink(m_densitySink,         persona.density,
            result.densityApplied,       result.lastError, "density");
    RunSink(m_blockStyleSink,      persona.blockStyle,
            result.blockStyleApplied,    result.lastError, "blockStyle");
    RunSink(m_toolbarSink,         persona.defaultToolbarCommands,
            result.toolbarApplied,       result.lastError, "toolbar");
    RunSink(m_panelVisibilitySink, persona.defaultPanels,
            result.panelVisibilityApplied, result.lastError, "panelVisibility");

    return result;
}

PersonaApplyResult PersonaActivator::ApplyActive(const PersonaRegistry& registry)
{
    PersonaApplyResult result;
    const UIPersona* active = registry.GetActive();
    if (active == nullptr)
    {
        result.lastError = "no active persona";
        return result;
    }
    return Apply(*active);
}

} // namespace SagaEditor
