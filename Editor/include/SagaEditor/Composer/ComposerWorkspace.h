/// @file ComposerWorkspace.h
/// @brief Workspace path contract for the SagaEditorComposer application.

#pragma once

#include <filesystem>

namespace SagaEditor
{

/// Canonical source and generated output paths consumed by SagaEditorComposer.
struct ComposerWorkspacePaths
{
    std::filesystem::path root;             ///< Repository or project root.
    std::filesystem::path sourceRoot;       ///< Canonical editor composition source root.
    std::filesystem::path artifactPath;     ///< Generated compiled composition artifact.
    std::filesystem::path manifestPath;     ///< Generated composition manifest.
    std::filesystem::path diagnosticsPath;  ///< Generated diagnostics report.
    std::filesystem::path sourceMapPath;    ///< Generated composition source map.
    std::filesystem::path dependenciesPath; ///< Generated dependency manifest.
    std::filesystem::path checkpointRoot;   ///< Composer source edit checkpoint directory.
};

/// Resolve product editor-composition paths from a project root.
[[nodiscard]] ComposerWorkspacePaths ResolveComposerWorkspacePaths(
    std::filesystem::path root);

} // namespace SagaEditor
