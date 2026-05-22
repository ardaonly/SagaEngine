/// @file ComposerArtifactSummary.h
/// @brief Generated editor composition artifact summary reader.

#pragma once

#include "SagaEditor/Composer/ComposerWorkspace.h"

#include <filesystem>
#include <string>

namespace SagaEditor
{

/// Read-only summary of generated editor composition outputs.
struct ComposerArtifactSummary
{
    bool manifestFound = false;       ///< True when manifest JSON exists.
    bool artifactFound = false;       ///< True when artifact JSON exists.
    bool diagnosticsFound = false;    ///< True when diagnostics report exists.
    bool sourceMapFound = false;      ///< True when source-map JSON exists.
    bool dependenciesFound = false;   ///< True when dependency manifest exists.
    std::string compositionId;        ///< Composition id from manifest when available.
    std::string artifactHash;         ///< Artifact hash from manifest when available.
    std::string message;              ///< Load status or error.
    int panelCount = 0;               ///< Compiled panel count.
    int actionCount = 0;              ///< Compiled action count.
    int menuCount = 0;                ///< Compiled menu count.
    int toolbarCount = 0;             ///< Compiled toolbar count.
    int shortcutCount = 0;            ///< Compiled shortcut count.
    int workspaceCount = 0;           ///< Compiled workspace count.
    int modeCount = 0;                ///< Compiled editor mode count.
};

/// Load generated composition manifest and artifact summary if present.
[[nodiscard]] ComposerArtifactSummary LoadComposerArtifactSummary(
    const ComposerWorkspacePaths& paths);

} // namespace SagaEditor
