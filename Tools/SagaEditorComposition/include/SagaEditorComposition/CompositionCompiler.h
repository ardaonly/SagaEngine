/// @file CompositionCompiler.h
/// @brief SDE-adjacent compiler for saga.editor composition artifacts.

#pragma once

#include "SDE/Compiler/CompileResult.h"
#include "SDE/Validation/Diagnostic.h"

#include <filesystem>
#include <string>
#include <vector>

namespace SagaEditorComposition
{

/// File paths produced by the saga.editor composition artifact writer.
struct CompositionOutputPaths
{
    std::filesystem::path artifactPath;
    std::filesystem::path manifestPath;
    std::filesystem::path diagnosticsPath;
    std::filesystem::path sourceMapPath;
    std::filesystem::path dependencyManifestPath;
};

/// Request for compiling a workspace into editor composition artifacts.
struct CompositionCompileRequest
{
    std::filesystem::path workspaceRoot;
    std::filesystem::path outputRoot;
    std::filesystem::path buildRoot;
    CompositionOutputPaths explicitOutputs;
    std::string compositionId;
};

/// Result of validating or compiling saga.editor composition artifacts.
struct CompositionCompileResult
{
    SDE::CompileState state = SDE::CompileState::Clean;
    std::vector<SDE::Diagnostic> diagnostics;
    SDE::CompilerHashes hashes;
    CompositionOutputPaths outputs;
};

/// Return the canonical product build output locations for saga.editor data.
[[nodiscard]] CompositionOutputPaths MakeBuildOutputPaths(
    const std::filesystem::path& buildRoot);

/// Compile SDE source and emit saga.editor composition artifact files.
class CompositionCompiler
{
public:
    /// Validate the requested composition without writing output files.
    [[nodiscard]] CompositionCompileResult Validate(
        const CompositionCompileRequest& request) const;

    /// Compile the requested composition and write deterministic output files.
    [[nodiscard]] CompositionCompileResult Compile(
        const CompositionCompileRequest& request) const;
};

} // namespace SagaEditorComposition
