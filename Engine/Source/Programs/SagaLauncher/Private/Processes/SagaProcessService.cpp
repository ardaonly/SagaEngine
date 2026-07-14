/// @file SagaProcessService.cpp
/// @brief Implements bounded Product Shell process execution without a command shell.

#include "Processes/SagaProcessService.h"

#include <QProcess>
#include <QProcessEnvironment>
#include <QStringList>

#include <algorithm>
#include <array>
#include <chrono>

namespace SagaProduct
{
namespace
{

using Clock = std::chrono::steady_clock;

constexpr std::chrono::milliseconds kMaximumTimeout{300000};
constexpr qsizetype kMaximumCapturedBytes = 1024 * 1024;

[[nodiscard]] QStringList ToQStringList(
    const std::vector<std::string>& arguments)
{
    QStringList result;
    for (const std::string& argument : arguments)
    {
        result.push_back(QString::fromStdString(argument));
    }
    return result;
}

[[nodiscard]] std::string BoundedText(const QByteArray& bytes)
{
    const qsizetype size = std::min(bytes.size(), kMaximumCapturedBytes);
    return std::string(bytes.constData(), static_cast<std::size_t>(size));
}

[[nodiscard]] std::string ExecutableBasename(
    const std::filesystem::path& executable)
{
    std::string basename = executable.filename().string();
#if defined(_WIN32)
    if (executable.extension() == ".exe")
    {
        basename = executable.stem().string();
    }
#endif
    return basename;
}

[[nodiscard]] bool IsUsableWorkingDirectory(
    const std::filesystem::path& path)
{
    return path.empty() || std::filesystem::is_directory(path);
}

[[nodiscard]] std::filesystem::path NormalizedAbsolutePath(
    const std::filesystem::path& path)
{
    std::error_code error;
    const std::filesystem::path absolute = std::filesystem::absolute(path, error);
    return error ? std::filesystem::path{} : absolute.lexically_normal();
}

} // namespace

const char* ToString(SagaProcessTargetId target) noexcept
{
    switch (target)
    {
        case SagaProcessTargetId::Editor: return "editor";
        case SagaProcessTargetId::Runtime: return "runtime";
        case SagaProcessTargetId::SagaProject: return "sagaproject";
        case SagaProcessTargetId::Forge: return "forge";
        case SagaProcessTargetId::SagaScript: return "sagascript";
    }
    return "unknown";
}

const char* ToString(SagaProcessExitClassification classification) noexcept
{
    switch (classification)
    {
        case SagaProcessExitClassification::NotStarted: return "not_started";
        case SagaProcessExitClassification::Detached: return "detached";
        case SagaProcessExitClassification::Succeeded: return "succeeded";
        case SagaProcessExitClassification::Failed: return "failed";
        case SagaProcessExitClassification::Crashed: return "crashed";
        case SagaProcessExitClassification::TimedOut: return "timed_out";
        case SagaProcessExitClassification::Cancelled: return "cancelled";
        case SagaProcessExitClassification::InvalidRequest: return "invalid_request";
    }
    return "unknown";
}

bool SagaProcessService::IsExecutableAllowed(
    SagaProcessTargetId target,
    const std::filesystem::path& executable) noexcept
{
    if (executable.empty())
    {
        return false;
    }
    const std::string basename = ExecutableBasename(executable);
    switch (target)
    {
        case SagaProcessTargetId::Editor:
            return basename == "SagaEditor";
        case SagaProcessTargetId::Runtime:
            return basename == "SagaRuntime";
        case SagaProcessTargetId::SagaProject:
            return basename == "sagaproject";
        case SagaProcessTargetId::Forge:
            return basename == "forge";
        case SagaProcessTargetId::SagaScript:
            return basename == "sagascript";
    }
    return false;
}

bool SagaProcessService::IsEnvironmentKeyAllowed(
    const std::string& key) noexcept
{
    static constexpr std::array<const char*, 3> kAllowedKeys = {
        "FONTCONFIG_PATH",
        "QT_QPA_PLATFORM",
        "SAGASCRIPT_RUNTIME_BRIDGE_ASSEMBLY",
    };
    return std::find(kAllowedKeys.begin(), kAllowedKeys.end(), key) !=
        kAllowedKeys.end();
}

SagaProductProcessResult SagaProcessService::Run(
    const SagaProductProcessRequest& request)
{
    SagaProductProcessResult result;
    const auto startedAt = Clock::now();

    if (!IsExecutableAllowed(request.target, request.executable))
    {
        result.classification = SagaProcessExitClassification::InvalidRequest;
        result.error = std::string("Executable is not allowed for target ") +
            ToString(request.target) + ": " + request.executable.string();
        return result;
    }
    const std::filesystem::path executable =
        NormalizedAbsolutePath(request.executable);
    if (executable.empty() || !std::filesystem::is_regular_file(executable))
    {
        result.classification = SagaProcessExitClassification::InvalidRequest;
        result.error = "Executable does not exist as a regular file: " +
            request.executable.string();
        return result;
    }
    const std::filesystem::path workingDirectory = request.workingDirectory.empty() ?
        std::filesystem::path{} : NormalizedAbsolutePath(request.workingDirectory);
    if (!IsUsableWorkingDirectory(workingDirectory))
    {
        result.classification = SagaProcessExitClassification::InvalidRequest;
        result.error = "Working directory does not exist: " +
            request.workingDirectory.string();
        return result;
    }
    if (request.mode == SagaProcessExecutionMode::WaitForCompletion &&
        (request.timeout.count() <= 0 || request.timeout > kMaximumTimeout))
    {
        result.classification = SagaProcessExitClassification::InvalidRequest;
        result.error = "Process timeout must be between 1 and 300000 milliseconds.";
        return result;
    }
    for (const auto& [key, value] : request.environment)
    {
        (void)value;
        if (!IsEnvironmentKeyAllowed(key))
        {
            result.classification = SagaProcessExitClassification::InvalidRequest;
            result.error = "Environment override is not allowed: " + key;
            return result;
        }
    }

    QProcess process;
    process.setProgram(QString::fromStdString(executable.string()));
    process.setArguments(ToQStringList(request.arguments));
    process.setProcessChannelMode(QProcess::SeparateChannels);
    if (!workingDirectory.empty())
    {
        process.setWorkingDirectory(
            QString::fromStdString(workingDirectory.string()));
    }
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    for (const auto& [key, value] : request.environment)
    {
        environment.insert(
            QString::fromStdString(key), QString::fromStdString(value));
    }
    process.setProcessEnvironment(environment);

    if (request.mode == SagaProcessExecutionMode::Detached)
    {
        qint64 processId = 0;
        result.started = process.startDetached(&processId);
        result.classification = result.started ?
            SagaProcessExitClassification::Detached :
            SagaProcessExitClassification::NotStarted;
        if (!result.started)
        {
            result.error = process.errorString().toStdString();
        }
        result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            Clock::now() - startedAt);
        return result;
    }

    process.start();
    if (!process.waitForStarted())
    {
        result.error = process.errorString().toStdString();
        result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            Clock::now() - startedAt);
        return result;
    }

    result.started = true;
    const auto deadline = startedAt + request.timeout;
    while (process.state() != QProcess::NotRunning)
    {
        if (request.stopToken.stop_requested())
        {
            result.cancelled = true;
            break;
        }
        if (Clock::now() >= deadline)
        {
            result.timedOut = true;
            break;
        }
        process.waitForFinished(50);
    }
    if (result.cancelled || result.timedOut)
    {
        process.terminate();
        if (!process.waitForFinished(750))
        {
            process.kill();
            process.waitForFinished(750);
        }
    }

    result.standardOutput = BoundedText(process.readAllStandardOutput());
    result.standardError = BoundedText(process.readAllStandardError());
    result.exitCode = process.exitCode();
    if (result.cancelled)
    {
        result.classification = SagaProcessExitClassification::Cancelled;
    }
    else if (result.timedOut)
    {
        result.classification = SagaProcessExitClassification::TimedOut;
    }
    else if (process.exitStatus() != QProcess::NormalExit)
    {
        result.classification = SagaProcessExitClassification::Crashed;
        if (result.exitCode == 0)
        {
            result.exitCode = 1;
        }
    }
    else if (result.exitCode == 0)
    {
        result.classification = SagaProcessExitClassification::Succeeded;
    }
    else
    {
        result.classification = SagaProcessExitClassification::Failed;
    }
    result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        Clock::now() - startedAt);
    return result;
}

} // namespace SagaProduct
