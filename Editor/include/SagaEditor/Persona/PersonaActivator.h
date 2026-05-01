/// @file PersonaActivator.h
/// @brief Fans a persona change out to theme, layout, density, and graph systems.

#pragma once

#include "SagaEditor/Persona/UIPersona.h"

#include <functional>
#include <string>
#include <vector>

namespace SagaEditor
{

class PersonaRegistry;

// ─── Activation Result ────────────────────────────────────────────────────────

/// Per-subsystem outcome of a persona apply pass. The activator
/// returns this so the welcome screen can show "applied: theme,
/// layout, density; failed: graph (no canvas yet)" instead of a
/// silent boolean.
struct PersonaApplyResult
{
    bool        themeApplied         = false;
    bool        layoutApplied        = false;
    bool        densityApplied       = false;
    bool        blockStyleApplied    = false;
    bool        toolbarApplied       = false;
    bool        panelVisibilityApplied = false;
    std::string lastError;          ///< Empty when every step succeeded.

    /// True when every wired subscriber reported success. A subsystem
    /// that has no subscriber is treated as "not wired" rather than as
    /// a failure (the welcome screen displays it as "not yet
    /// available" so users do not think they did something wrong).
    [[nodiscard]] bool AllApplied() const noexcept
    {
        return themeApplied && layoutApplied && densityApplied &&
               blockStyleApplied && toolbarApplied && panelVisibilityApplied;
    }

    /// True when at least one subsystem was actually swapped. Used by
    /// the View menu to decide whether to flash the toolbar to confirm
    /// the change.
    [[nodiscard]] bool AnyApplied() const noexcept
    {
        return themeApplied || layoutApplied || densityApplied ||
               blockStyleApplied || toolbarApplied || panelVisibilityApplied;
    }
};

// ─── Persona Activator ────────────────────────────────────────────────────────

/// Decouples the persona registry from the concrete subsystems it
/// drives. Each consumer (the theme registry, the layout serializer,
/// the inspector density service, the graph canvas, the toolbar
/// manager, the panel manager) installs a sink through `Set*Sink`.
/// `Apply` walks the persona once and invokes the installed sinks;
/// uninstalled sinks are silently skipped. The class never owns the
/// subsystems; it stores `std::function`s the host wires up at
/// startup.
class PersonaActivator
{
public:
    using ThemeSink           = std::function<bool(const std::string& themeId)>;
    using LayoutSink          = std::function<bool(const std::string& presetId)>;
    using DensitySink         = std::function<bool(const DensityProfile&)>;
    using BlockStyleSink      = std::function<bool(const BlockVisualStyle&)>;
    using ToolbarSink         = std::function<bool(const std::vector<std::string>& commands)>;
    using PanelVisibilitySink = std::function<bool(const std::vector<std::string>& panels)>;

    PersonaActivator()  = default;
    ~PersonaActivator() = default;

    PersonaActivator(const PersonaActivator&)            = delete;
    PersonaActivator& operator=(const PersonaActivator&) = delete;
    PersonaActivator(PersonaActivator&&)                 = default;
    PersonaActivator& operator=(PersonaActivator&&)      = default;

    // ─── Sink Installation ────────────────────────────────────────────────────

    void SetThemeSink          (ThemeSink           sink) noexcept;
    void SetLayoutSink         (LayoutSink          sink) noexcept;
    void SetDensitySink        (DensitySink         sink) noexcept;
    void SetBlockStyleSink     (BlockStyleSink      sink) noexcept;
    void SetToolbarSink        (ToolbarSink         sink) noexcept;
    void SetPanelVisibilitySink(PanelVisibilitySink sink) noexcept;

    /// Drop every sink. Used by tests between cases.
    void ClearSinks() noexcept;

    // ─── Apply ────────────────────────────────────────────────────────────────

    /// Apply `persona` by invoking every installed sink in order:
    /// theme → layout → density → block style → toolbar → panel
    /// visibility. Sink return values feed into `PersonaApplyResult`.
    /// A sink that returns false sets `lastError` but does not abort
    /// the rest of the pass — applying a partial persona is better
    /// than rolling back to the previous one mid-flight.
    [[nodiscard]] PersonaApplyResult Apply(const UIPersona& persona);

    /// Pull the currently-active persona from `registry` and apply it.
    /// Returns the same result struct as the per-persona overload.
    /// When no persona is active, every flag stays false and
    /// `lastError` is set to a diagnostic message.
    [[nodiscard]] PersonaApplyResult ApplyActive(const PersonaRegistry& registry);

private:
    ThemeSink           m_themeSink;
    LayoutSink          m_layoutSink;
    DensitySink         m_densitySink;
    BlockStyleSink      m_blockStyleSink;
    ToolbarSink         m_toolbarSink;
    PanelVisibilitySink m_panelVisibilitySink;
};

} // namespace SagaEditor
