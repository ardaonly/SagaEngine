/// @file ComposerBuildPlan.h
/// @brief External SagaPipeline build command planner for SagaEditorComposer.

#pragma once

#include "SagaEditor/Composer/ComposerWorkspace.h"

#include <filesystem>
#include <string>
#include <vector>

namespace SagaEditor
{

/// Deterministic command plan for invoking SagaPipeline as an external process.
struct ComposerBuildPlan
{
    std::filesystem::path executable; ///< saga-pipeline executable path or command name.
    std::vector<std::string> arguments; ///< Ordered process arguments.
    std::filesystem::path compilerPath; ///< saga-editor-composition-compiler path or command name.
    std::string displayCommand;         ///< Shell-style command shown to the user.
};

/// Resolve a build command using sibling executable discovery with PATH fallback.
[[nodiscard]] ComposerBuildPlan MakeComposerBuildPlan(
    const ComposerWorkspacePaths& paths,
    const std::filesystem::path& composerExecutablePath = {});

} // namespace SagaEditor
