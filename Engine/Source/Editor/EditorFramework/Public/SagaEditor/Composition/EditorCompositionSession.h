/// @file EditorCompositionSession.h
/// @brief Startup session state for compiled editor composition consumption.

#pragma once

#include "SagaEditor/Composition/EditorCompositionDiagnostics.h"
#include "SagaEditor/Composition/ResolvedEditorCompositionSnapshot.h"

#include <filesystem>
#include <optional>
#include <vector>

namespace SagaEditor
{

/// Startup configuration for consuming compiled saga.editor composition output.
struct EditorCompositionStartupConfig
{
    std::filesystem::path manifestPath;              ///< Compiled composition manifest path.
    std::vector<std::filesystem::path> overlayPaths; ///< Optional safe user overlay files.
    bool requireValidComposition = false;            ///< Fail host startup when no usable snapshot exists.

    /// Return true when startup should attempt composition loading.
    [[nodiscard]] bool IsConfigured() const noexcept
    {
        return !manifestPath.empty() || !overlayPaths.empty();
    }
};

/// High-level state of the composition startup session.
enum class EditorCompositionSessionStatus
{
    Unconfigured,
    Loaded,
    Failed,
};

/// Owns the resolved editor composition snapshot for the current editor session.
class EditorCompositionSession
{
public:
    /// Load compiled composition output and resolve safe overlays.
    [[nodiscard]] bool Init(const EditorCompositionStartupConfig& config);

    /// Clear loaded state and diagnostics.
    void Shutdown();

    [[nodiscard]] const EditorCompositionStartupConfig& Config() const noexcept;
    [[nodiscard]] EditorCompositionSessionStatus Status() const noexcept;
    [[nodiscard]] bool IsUsable() const noexcept;
    [[nodiscard]] const std::optional<ResolvedEditorCompositionSnapshot>&
        Snapshot() const noexcept;
    [[nodiscard]] const std::vector<EditorCompositionDiagnostic>&
        Diagnostics() const noexcept;

private:
    EditorCompositionStartupConfig m_config;
    EditorCompositionSessionStatus m_status = EditorCompositionSessionStatus::Unconfigured;
    std::optional<ResolvedEditorCompositionSnapshot> m_snapshot;
    std::vector<EditorCompositionDiagnostic> m_diagnostics;
};

} // namespace SagaEditor
