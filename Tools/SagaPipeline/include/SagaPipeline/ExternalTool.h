/// @file ExternalTool.h
/// @brief External process invocation contract for SagaPipeline.

#pragma once

#include <string>
#include <vector>

namespace SagaPipeline
{

/// Fully planned external tool invocation.
struct ExternalToolInvocation
{
    std::string executable; ///< Tool executable name or path.
    std::vector<std::string> arguments; ///< Arguments passed to the executable.
};

/// Low-level result of spawning an external tool.
struct ExternalToolResult
{
    int exitCode = 0; ///< Process exit code, or -1 when launch failed.
    std::string error; ///< Launch error when exitCode is -1.
};

/// Interface used to run external tools and fake invocation in tests.
class IExternalToolRunner
{
public:
    virtual ~IExternalToolRunner() = default;

    /// Run the invocation and return the child process status.
    [[nodiscard]] virtual ExternalToolResult Run(
        const ExternalToolInvocation& invocation) const = 0;
};

/// Default inherited-stdio external process runner.
class ProcessExternalToolRunner final : public IExternalToolRunner
{
public:
    /// Spawn the external tool and wait for completion.
    [[nodiscard]] ExternalToolResult Run(
        const ExternalToolInvocation& invocation) const override;
};

} // namespace SagaPipeline
