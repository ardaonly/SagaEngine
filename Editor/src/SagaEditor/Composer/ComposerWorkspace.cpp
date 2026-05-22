/// @file ComposerWorkspace.cpp
/// @brief Workspace path contract implementation for SagaEditorComposer.

#include "SagaEditor/Composer/ComposerWorkspace.h"

#include <utility>

namespace SagaEditor
{

ComposerWorkspacePaths ResolveComposerWorkspacePaths(std::filesystem::path root)
{
    if (root.empty())
    {
        root = std::filesystem::current_path();
    }

    ComposerWorkspacePaths paths;
    paths.root = std::move(root);
    paths.sourceRoot = paths.root / "Editor" / "CompositionSources";
    paths.artifactPath = paths.root / "Build" / "Artifacts" /
                         "EditorComposition" /
                         "saga.editor.composition.json";
    paths.manifestPath = paths.root / "Build" / "Manifests" /
                         "saga.editor.composition.manifest.json";
    paths.diagnosticsPath = paths.root / "Build" / "Reports" /
                            "saga.editor.composition.diagnostics.json";
    paths.sourceMapPath = paths.root / "Build" / "Manifests" /
                          "saga.editor.composition.sourcemap.json";
    paths.dependenciesPath = paths.root / "Build" / "Manifests" /
                             "saga.editor.composition.dependencies.json";
    paths.checkpointRoot = paths.root / ".saga" / "composer" / "checkpoints";
    return paths;
}

} // namespace SagaEditor
