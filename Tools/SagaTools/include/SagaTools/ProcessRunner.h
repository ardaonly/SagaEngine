/// @file ProcessRunner.h
/// @brief Cross-platform synchronous child-process spawn used by the dispatcher.

#pragma once

#include <string>
#include <vector>

namespace SagaTools
{

// ─── Process Runner ───────────────────────────────────────────────────────────

/// Launch an executable, inherit stdio, return its exit code.
///
/// This is the dispatcher's *only* mechanism for invoking another tool —
/// every child process is launched through this one entry point. Capturing
/// stdout/stderr is intentionally not supported; the user invoked
/// `tools <name>` because they wanted that tool's output unmodified.
///
/// Returns -1 on spawn failure and writes a reason to `outError` when
/// non-null.
class ProcessRunner
{
public:
    [[nodiscard]] static int Run(const std::string&              executable,
                                 const std::vector<std::string>& args,
                                 std::string*                    outError) noexcept;
};

} // namespace SagaTools
