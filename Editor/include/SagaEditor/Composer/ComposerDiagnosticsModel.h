/// @file ComposerDiagnosticsModel.h
/// @brief Diagnostics report model for SagaEditorComposer.

#pragma once

#include "SagaEditor/Composer/ComposerWorkspace.h"

#include <filesystem>
#include <string>
#include <vector>

namespace SagaEditor
{

/// One diagnostic row loaded from a generated composition diagnostics report.
struct ComposerDiagnosticRow
{
    std::string severity;          ///< Diagnostic severity text.
    std::string code;              ///< Stable diagnostic code.
    std::string message;           ///< Diagnostic message.
    std::filesystem::path file;    ///< Source file when present.
    int line = 0;                  ///< One-based source line when present.
    int column = 0;                ///< One-based source column when present.
};

/// Loaded diagnostics report state.
struct ComposerDiagnosticsModel
{
    bool reportFound = false;                    ///< True when diagnostics report exists.
    std::string message;                         ///< Load status or error.
    std::vector<ComposerDiagnosticRow> rows;     ///< Diagnostic rows.
};

/// Load generated composition diagnostics JSON if present.
[[nodiscard]] ComposerDiagnosticsModel LoadComposerDiagnostics(
    const ComposerWorkspacePaths& paths);

} // namespace SagaEditor
