/// @file SagaPipelineConfig.h
/// @brief Saga-specific default paths and executable names for pipeline steps.

#pragma once

#include <filesystem>
#include <string>

namespace SagaPipeline
{

/// Central SagaPipeline default configuration.
struct SagaPipelineConfig
{
    std::filesystem::path projectRoot = "."; ///< Saga project root.
    std::string editorCompositionTool = "saga-editor-composition-compiler"; ///< Default editor composition compiler.
    std::string editorCompositionId = "saga.editor.default"; ///< Default product editor composition id.
};

/// Return the default editor composition source workspace for a project.
[[nodiscard]] std::filesystem::path DefaultEditorCompositionWorkspace(
    const std::filesystem::path& projectRoot);

/// Return the default Saga product build root for a project.
[[nodiscard]] std::filesystem::path DefaultBuildRoot(
    const std::filesystem::path& projectRoot);

} // namespace SagaPipeline
