/// @file ProcessRunner.h
/// @brief Internal cross-platform child-process spawn used by Forge subcommands.

#pragma once

#include <string>
#include <vector>

namespace Forge
{

// ─── Process Runner ───────────────────────────────────────────────────────────

/// Synchronously spawn a child process and forward its exit code.
///
/// This helper is private to Forge — it is intentionally NOT shipped as a
/// public library API. Forge does not expose a "run any command" service to
/// other tools; that would invite Forge to grow into a generic orchestrator,
/// which is explicitly out of scope.
///
/// Stdio is inherited from the parent. On spawn failure the function returns
/// `-1` and writes a human-readable reason to `outError` when non-null.
class ProcessRunner
{
public:
    [[nodiscard]] static int Run(const std::string&              executable,
                                 const std::vector<std::string>& args,
                                 std::string*                    outError) noexcept;
};

} // namespace Forge
