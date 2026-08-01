// SPDX-License-Identifier: MPL-2.0
#pragma once

#include "SagaEditor/Processes/ToolProcessRunner.h"

#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>

namespace SagaEditor
{

enum class ToolProcessSessionState
{
    Stopped,
    Running,
    Exited,
    Crashed,
    ProtocolLimitExceeded,
};

struct ToolProcessSessionOptions
{
    ToolProcessRequest process;
    std::size_t messageLimitBytes = 4 * 1024 * 1024;
    std::size_t queueLimitMessages = 256;
};

/// Persistent bidirectional child process with owned stdin/stdout/stderr.
/// Lines are bounded before publication; destruction always reaps the child.
class ToolProcessSession final
{
public:
    ToolProcessSession();
    ~ToolProcessSession();
    ToolProcessSession(const ToolProcessSession&) = delete;
    ToolProcessSession& operator=(const ToolProcessSession&) = delete;

    [[nodiscard]] bool Start(const ToolProcessSessionOptions& options);
    [[nodiscard]] bool WriteLine(const std::string& line);
    [[nodiscard]] std::optional<std::string> ReadLine(std::chrono::milliseconds timeout);
    [[nodiscard]] std::string TakeStandardError();
    [[nodiscard]] ToolProcessSessionState State() const noexcept;
    [[nodiscard]] int ExitCode() const noexcept;
    [[nodiscard]] std::uint64_t ProcessId() const noexcept;
    [[nodiscard]] std::string Diagnostic() const;
    void Shutdown(std::chrono::milliseconds graceful = std::chrono::milliseconds{1000},
                  std::chrono::milliseconds terminate = std::chrono::milliseconds{1000});

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace SagaEditor
