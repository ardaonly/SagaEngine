/// @file EditorCompositionStep.h
/// @brief Saga-specific orchestration for editor composition artifact builds.

#pragma once

#include "SagaPipeline/ExternalTool.h"
#include "SagaPipeline/PipelineResult.h"

#include <filesystem>
#include <string>

namespace SagaPipeline
{

/// Request for the Saga editor composition build step.
struct EditorCompositionBuildRequest
{
    std::filesystem::path projectRoot = "."; ///< Saga project root.
    std::filesystem::path workspaceRoot; ///< Optional explicit source workspace.
    std::filesystem::path buildRoot; ///< Optional explicit build root.
    std::string toolExecutable = "saga-editor-composition-compiler"; ///< Compiler executable.
    std::string compositionId = "saga.editor.default"; ///< Composition root id.
    bool explain = false; ///< Plan without invoking the compiler.
};

/// Result of planning or running the editor composition build step.
struct EditorCompositionBuildResult
{
    PipelineResult pipeline; ///< Step status and diagnostics.
    ExternalToolInvocation invocation; ///< Compiler invocation.
    std::filesystem::path workspaceRoot; ///< Resolved workspace path.
    std::filesystem::path buildRoot; ///< Resolved build root.
};

/// Builds and optionally runs the SagaEditor composition compiler invocation.
class EditorCompositionStep
{
public:
    /// Return the compiler invocation without running it.
    [[nodiscard]] static EditorCompositionBuildResult Plan(
        const EditorCompositionBuildRequest& request);

    /// Run the compiler unless explain mode is requested.
    [[nodiscard]] static EditorCompositionBuildResult Run(
        const EditorCompositionBuildRequest& request,
        const IExternalToolRunner& runner);
};

} // namespace SagaPipeline
