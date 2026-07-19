// SPDX-License-Identifier: MPL-2.0

#pragma once

#include <chrono>
#include <filesystem>
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace SagaEditor
{

enum class ToolProcessExitClassification
{
    Succeeded,
    Failed,
    NotStarted,
    TimedOut,
    Cancelled,
    Crashed,
};

enum class ToolProcessShutdownClassification
{
    None,
    Graceful,
    Terminated,
    Forced,
};

struct ToolProcessRequest
{
    std::filesystem::path executable;
    std::vector<std::string> arguments;
    std::filesystem::path workingDirectory;
    std::map<std::string, std::string> environment;
    bool inheritEnvironment = true;
    std::chrono::milliseconds timeout{30000};
    std::size_t outputLimitBytes = 1024 * 1024;
    std::function<bool()> cancellationRequested;
    std::function<void()> requestCancellation;
    std::function<void()> onPoll;
    std::function<std::chrono::milliseconds()> cancellationGraceOverride;
    std::chrono::milliseconds cancellationGrace{2000};
    std::chrono::milliseconds terminationGrace{1000};
    std::chrono::milliseconds forceKillGrace{1000};
};

struct ToolProcessResult
{
    ToolProcessExitClassification classification = ToolProcessExitClassification::NotStarted;
    ToolProcessShutdownClassification shutdownClassification =
        ToolProcessShutdownClassification::None;
    int exitCode = -1;
    std::string standardOutput;
    std::string standardError;
    std::string diagnosticCode;
    std::string diagnosticMessage;
    std::chrono::milliseconds duration{};

    [[nodiscard]] bool Succeeded() const noexcept
    {
        return classification == ToolProcessExitClassification::Succeeded;
    }
};

class ToolProcessRunner
{
public:
    [[nodiscard]] ToolProcessResult Run(const ToolProcessRequest& request) const;
};

} // namespace SagaEditor
