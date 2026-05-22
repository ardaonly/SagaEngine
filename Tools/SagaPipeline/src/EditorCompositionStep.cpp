/// @file EditorCompositionStep.cpp
/// @brief SagaEditor composition compiler orchestration step.

#include "SagaPipeline/EditorCompositionStep.h"

#include "SagaPipeline/SagaPipelineConfig.h"

namespace SagaPipeline
{

EditorCompositionBuildResult EditorCompositionStep::Plan(
    const EditorCompositionBuildRequest& request)
{
    EditorCompositionBuildResult result;
    result.workspaceRoot = request.workspaceRoot.empty()
        ? DefaultEditorCompositionWorkspace(request.projectRoot)
        : request.workspaceRoot.lexically_normal();
    result.buildRoot = request.buildRoot.empty()
        ? DefaultBuildRoot(request.projectRoot)
        : request.buildRoot.lexically_normal();

    result.invocation.executable = request.toolExecutable.empty()
        ? "saga-editor-composition-compiler"
        : request.toolExecutable;
    result.invocation.arguments = {
        "compile",
        "--workspace",
        result.workspaceRoot.generic_string(),
        "--build-root",
        result.buildRoot.generic_string(),
        "--composition-id",
        request.compositionId.empty() ? "saga.editor.default" : request.compositionId,
    };

    return result;
}

EditorCompositionBuildResult EditorCompositionStep::Run(
    const EditorCompositionBuildRequest& request,
    const IExternalToolRunner& runner)
{
    EditorCompositionBuildResult result = Plan(request);
    if (request.explain)
    {
        return result;
    }

    const ExternalToolResult toolResult = runner.Run(result.invocation);
    result.pipeline.exitCode = toolResult.exitCode;
    if (toolResult.exitCode < 0)
    {
        result.pipeline.diagnostics.push_back({
            PipelineDiagnosticSeverity::Error,
            "SAGA_PIPELINE_TOOL_LAUNCH_FAILED",
            toolResult.error.empty()
                ? "Could not launch external editor composition compiler."
                : toolResult.error,
        });
    }
    else if (toolResult.exitCode != 0)
    {
        result.pipeline.diagnostics.push_back({
            PipelineDiagnosticSeverity::Error,
            "SAGA_PIPELINE_TOOL_EXIT_FAILED",
            "Editor composition compiler exited with a non-zero status.",
        });
    }

    return result;
}

} // namespace SagaPipeline
