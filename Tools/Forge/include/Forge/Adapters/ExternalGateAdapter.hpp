/// @file ExternalGateAdapter.hpp
/// @brief Generic external-tool gate adapter for Forge workflows.

#pragma once

#include <string>
#include <vector>

namespace Forge::Adapters
{

/// Options for invoking an external validation or generation gate.
struct ExternalGateOptions
{
    std::string              name;                 ///< Human-readable gate name.
    std::string              toolExecutable;       ///< Executable name or path.
    std::vector<std::string> toolArguments;        ///< Arguments passed through unchanged.
    std::string              diagnosticsPath;      ///< Machine-readable diagnostics report.
    std::string              blockingKey = "hasBlockingDiagnostics"; ///< Boolean blocker key.
    bool                     explain = false;      ///< Print command without running it.
};

/// Result of an external gate invocation.
struct ExternalGateResult
{
    int  toolExitCode = 0;              ///< Raw child process exit code.
    bool diagnosticsRead = false;       ///< True when diagnostics were consumed.
    bool hasBlockingDiagnostics = false; ///< True when the configured blocker key is true.
};

/// Runs an external tool and evaluates its diagnostics report.
class ExternalGateAdapter
{
public:
    /// Invoke the configured tool and read the diagnostics blocker key.
    [[nodiscard]] static ExternalGateResult Run(const ExternalGateOptions& options,
                                                std::string&               outError);
};

} // namespace Forge::Adapters
