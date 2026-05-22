/// @file EditorCompositionArtifactLoader.h
/// @brief Loads compiled SDE editor composition manifests and artifacts.

#pragma once

#include "SagaEditor/Composition/EditorCompositionArtifact.h"
#include "SagaEditor/Composition/EditorCompositionDiagnostics.h"
#include "SagaEditor/Composition/EditorCompositionManifest.h"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace SagaEditor
{

/// Result of loading an editor composition manifest.
struct EditorCompositionManifestLoadResult
{
    std::optional<EditorCompositionManifest> manifest;
    std::vector<EditorCompositionDiagnostic> diagnostics;
};

/// Result of loading a compiled editor composition artifact.
struct EditorCompositionArtifactLoadResult
{
    std::optional<EditorCompositionArtifact> artifact;
    std::vector<EditorCompositionDiagnostic> diagnostics;
};

/// Result of loading an artifact through its manifest.
struct EditorCompositionLoadResult
{
    std::optional<EditorCompositionManifest> manifest;
    std::optional<EditorCompositionArtifact> artifact;
    std::vector<EditorCompositionDiagnostic> diagnostics;
};

/// Loader for SDE-emitted editor composition JSON artifacts and manifests.
class EditorCompositionArtifactLoader
{
public:
    [[nodiscard]] EditorCompositionManifestLoadResult LoadManifestFile(
        const std::filesystem::path& path) const;

    [[nodiscard]] EditorCompositionManifestLoadResult LoadManifestText(
        const std::string& text,
        std::string documentPath = {}) const;

    [[nodiscard]] EditorCompositionArtifactLoadResult LoadArtifactFile(
        const std::filesystem::path& path) const;

    [[nodiscard]] EditorCompositionArtifactLoadResult LoadArtifactText(
        const std::string& text,
        std::string documentPath = {}) const;

    [[nodiscard]] EditorCompositionLoadResult LoadFromManifestFile(
        const std::filesystem::path& path) const;
};

} // namespace SagaEditor
