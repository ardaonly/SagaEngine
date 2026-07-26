// SPDX-License-Identifier: MPL-2.0

#include "SagaEditor/Processes/ToolProcessRunner.h"

#include <algorithm>
#include <array>
#include <thread>

#ifdef _WIN32
#include <windows.h>
#include <processthreadsapi.h>
#else
#include <cerrno>
#include <csignal>
#include <cstring>
#include <fcntl.h>
#include <poll.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace SagaEditor
{
namespace
{
void AppendBounded(std::string& destination,
                   const char* bytes,
                   std::size_t count,
                   std::size_t limit)
{
    if (destination.size() >= limit) return;
    destination.append(bytes, std::min(count, limit - destination.size()));
}

#ifdef _WIN32
std::string QuoteWindowsArgument(const std::string& argument)
{
    if (argument.find_first_of(" \t\"") == std::string::npos) return argument;
    std::string quoted = "\"";
    std::size_t slashes = 0;
    for (const char character : argument)
    {
        if (character == '\\') { ++slashes; continue; }
        if (character == '"')
        {
            quoted.append(slashes * 2 + 1, '\\');
            quoted.push_back('"');
        }
        else
        {
            quoted.append(slashes, '\\');
            quoted.push_back(character);
        }
        slashes = 0;
    }
    quoted.append(slashes * 2, '\\');
    quoted.push_back('"');
    return quoted;
}

bool MakeWindowsPipe(HANDLE& readHandle, HANDLE& writeHandle)
{
    SECURITY_ATTRIBUTES attributes{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
    if (!CreatePipe(&readHandle, &writeHandle, &attributes, 0)) return false;
    if (!SetHandleInformation(readHandle, HANDLE_FLAG_INHERIT, 0))
    {
        CloseHandle(readHandle); CloseHandle(writeHandle);
        readHandle = writeHandle = INVALID_HANDLE_VALUE;
        return false;
    }
    return true;
}

void DrainWindowsPipe(HANDLE pipe, std::string& output, std::size_t limit)
{
    std::array<char, 8192> buffer{};
    for (;;)
    {
        DWORD available = 0;
        if (!PeekNamedPipe(pipe, nullptr, 0, nullptr, &available, nullptr) || available == 0)
            return;
        DWORD read = 0;
        if (!ReadFile(pipe, buffer.data(),
            std::min<DWORD>(available, static_cast<DWORD>(buffer.size())), &read, nullptr) || read == 0)
            return;
        AppendBounded(output, buffer.data(), read, limit);
    }
}

std::vector<char> WindowsEnvironment(const ToolProcessRequest& request)
{
    if (request.environment.empty() && request.inheritEnvironment) return {};
    std::map<std::string, std::string> environment;
    if (request.inheritEnvironment)
    {
        LPCH values = GetEnvironmentStringsA();
        if (values)
        {
            for (const char* cursor = values; *cursor; cursor += std::strlen(cursor) + 1)
            {
                const std::string entry(cursor);
                const auto split = entry.find('=');
                if (split != std::string::npos && split != 0)
                    environment[entry.substr(0, split)] = entry.substr(split + 1);
            }
            FreeEnvironmentStringsA(values);
        }
    }
    for (const auto& [key, value] : request.environment) environment[key] = value;
    std::vector<char> block;
    for (const auto& [key, value] : environment)
    {
        const auto entry = key + "=" + value;
        block.insert(block.end(), entry.begin(), entry.end());
        block.push_back('\0');
    }
    block.push_back('\0');
    return block;
}
#else
bool MakePipe(std::array<int, 2>& pipe)
{
    if (::pipe(pipe.data()) != 0) return false;
    ::fcntl(pipe[0], F_SETFL, ::fcntl(pipe[0], F_GETFL) | O_NONBLOCK);
    return true;
}

void Drain(int fd, std::string& output, std::size_t limit, bool& open)
{
    std::array<char, 8192> buffer{};
    for (;;)
    {
        const auto count = ::read(fd, buffer.data(), buffer.size());
        if (count > 0)
        {
            AppendBounded(output, buffer.data(), static_cast<std::size_t>(count), limit);
            continue;
        }
        if (count == 0)
        {
            ::close(fd);
            open = false;
        }
        break;
    }
}
#endif
}

ToolProcessResult ToolProcessRunner::Run(const ToolProcessRequest& request) const
{
    ToolProcessResult result;
    const auto startedAt = std::chrono::steady_clock::now();
    if (request.executable.empty() || request.timeout <= std::chrono::milliseconds::zero())
    {
        result.diagnosticCode = "Editor.Process.InvalidRequest";
        result.diagnosticMessage = "Executable and a positive timeout are required.";
        return result;
    }

#ifdef _WIN32
    HANDLE stdoutRead = INVALID_HANDLE_VALUE, stdoutWrite = INVALID_HANDLE_VALUE;
    HANDLE stderrRead = INVALID_HANDLE_VALUE, stderrWrite = INVALID_HANDLE_VALUE;
    if (!MakeWindowsPipe(stdoutRead, stdoutWrite) ||
        !MakeWindowsPipe(stderrRead, stderrWrite))
    {
        if (stdoutRead != INVALID_HANDLE_VALUE) CloseHandle(stdoutRead);
        if (stdoutWrite != INVALID_HANDLE_VALUE) CloseHandle(stdoutWrite);
        if (stderrRead != INVALID_HANDLE_VALUE) CloseHandle(stderrRead);
        if (stderrWrite != INVALID_HANDLE_VALUE) CloseHandle(stderrWrite);
        result.diagnosticCode = "Editor.Process.PipeFailed";
        result.diagnosticMessage = std::to_string(GetLastError());
        return result;
    }
    std::string command = QuoteWindowsArgument(request.executable.string());
    for (const auto& argument : request.arguments)
        command += " " + QuoteWindowsArgument(argument);
    std::vector<char> commandLine(command.begin(), command.end());
    commandLine.push_back('\0');
    auto environment = WindowsEnvironment(request);
    STARTUPINFOA startup{};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdOutput = stdoutWrite;
    startup.hStdError = stderrWrite;
    startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    PROCESS_INFORMATION process{};
    const auto workingDirectory = request.workingDirectory.string();
    const BOOL created = CreateProcessA(nullptr, commandLine.data(), nullptr, nullptr, TRUE,
        CREATE_NO_WINDOW | CREATE_NEW_PROCESS_GROUP,
        environment.empty() ? nullptr : environment.data(),
        workingDirectory.empty() ? nullptr : workingDirectory.c_str(), &startup, &process);
    CloseHandle(stdoutWrite); CloseHandle(stderrWrite);
    if (!created)
    {
        CloseHandle(stdoutRead); CloseHandle(stderrRead);
        result.diagnosticCode = "Editor.Process.StartFailed";
        result.diagnosticMessage = std::to_string(GetLastError());
        return result;
    }
    HANDLE job = CreateJobObjectA(nullptr, nullptr);
    if (job)
    {
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
        limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        SetInformationJobObject(job, JobObjectExtendedLimitInformation, &limits, sizeof(limits));
        AssignProcessToJobObject(job, process.hProcess);
    }
    const auto deadline = startedAt + request.timeout;
    bool cancellationForwarded = false;
    auto cancellationDeadline = deadline;
    bool terminated = false;
    auto forceDeadline = deadline;
    for (;;)
    {
        DrainWindowsPipe(stdoutRead, result.standardOutput, request.outputLimitBytes);
        DrainWindowsPipe(stderrRead, result.standardError, request.outputLimitBytes);
        if (request.onPoll) request.onPoll();
        const auto now = std::chrono::steady_clock::now();
        if (request.cancellationRequested && request.cancellationRequested())
        {
            if (!cancellationForwarded)
            {
                cancellationForwarded = true;
                const auto grace = request.cancellationGraceOverride ?
                    request.cancellationGraceOverride() : request.cancellationGrace;
                cancellationDeadline = now + std::max(grace, std::chrono::milliseconds::zero());
                if (request.requestCancellation) request.requestCancellation();
            }
            if ((!request.requestCancellation || now >= cancellationDeadline) && !terminated)
            {
                result.classification = ToolProcessExitClassification::Cancelled;
                if (job) TerminateJobObject(job, 1); else TerminateProcess(process.hProcess, 1);
                result.shutdownClassification = ToolProcessShutdownClassification::Forced;
                terminated = true;
                forceDeadline = now + request.forceKillGrace;
            }
        }
        else if (now >= deadline && !terminated)
        {
            result.classification = ToolProcessExitClassification::TimedOut;
            if (job) TerminateJobObject(job, 1); else TerminateProcess(process.hProcess, 1);
            result.shutdownClassification = ToolProcessShutdownClassification::Forced;
            terminated = true;
            forceDeadline = now + request.forceKillGrace;
        }
        if (WaitForSingleObject(process.hProcess, 25) == WAIT_OBJECT_0) break;
        if (terminated && std::chrono::steady_clock::now() >= forceDeadline)
        {
            // Closing a kill-on-close Job Object is the final bounded
            // process-tree fallback. Never wait on the original run deadline.
            if (job) { CloseHandle(job); job = nullptr; }
            else TerminateProcess(process.hProcess, 1);
            break;
        }
    }
    DrainWindowsPipe(stdoutRead, result.standardOutput, request.outputLimitBytes);
    DrainWindowsPipe(stderrRead, result.standardError, request.outputLimitBytes);
    DWORD exitCode = 1;
    GetExitCodeProcess(process.hProcess, &exitCode);
    result.exitCode = exitCode == STILL_ACTIVE ? -1 : static_cast<int>(exitCode);
    if (result.classification != ToolProcessExitClassification::Cancelled &&
        result.classification != ToolProcessExitClassification::TimedOut)
        result.classification = exitCode == 0 ? ToolProcessExitClassification::Succeeded :
            ToolProcessExitClassification::Failed;
    else
        result.diagnosticCode = result.classification == ToolProcessExitClassification::TimedOut ?
            "Editor.Process.TimedOut" : "Editor.Process.Cancelled";
    CloseHandle(stdoutRead); CloseHandle(stderrRead);
    CloseHandle(process.hThread); CloseHandle(process.hProcess);
    if (job) CloseHandle(job);
#else
    std::array<int, 2> stdoutPipe{-1, -1};
    std::array<int, 2> stderrPipe{-1, -1};
    if (!MakePipe(stdoutPipe) || !MakePipe(stderrPipe))
    {
        result.diagnosticCode = "Editor.Process.PipeFailed";
        result.diagnosticMessage = std::strerror(errno);
        return result;
    }

    const pid_t pid = ::fork();
    if (pid == 0)
    {
        ::setpgid(0, 0);
        ::dup2(stdoutPipe[1], STDOUT_FILENO);
        ::dup2(stderrPipe[1], STDERR_FILENO);
        ::close(stdoutPipe[0]); ::close(stdoutPipe[1]);
        ::close(stderrPipe[0]); ::close(stderrPipe[1]);
        if (!request.workingDirectory.empty() &&
            ::chdir(request.workingDirectory.c_str()) != 0)
            _exit(126);
        if (!request.inheritEnvironment)
            ::clearenv();
        for (const auto& [key, value] : request.environment)
            ::setenv(key.c_str(), value.c_str(), 1);
        std::vector<std::string> storage;
        storage.reserve(request.arguments.size() + 1);
        storage.push_back(request.executable.string());
        storage.insert(storage.end(), request.arguments.begin(), request.arguments.end());
        std::vector<char*> argv;
        argv.reserve(storage.size() + 1);
        for (auto& item : storage) argv.push_back(item.data());
        argv.push_back(nullptr);
        ::execvp(argv[0], argv.data());
        _exit(127);
    }
    ::close(stdoutPipe[1]);
    ::close(stderrPipe[1]);
    if (pid < 0)
    {
        ::close(stdoutPipe[0]); ::close(stderrPipe[0]);
        result.diagnosticCode = "Editor.Process.StartFailed";
        result.diagnosticMessage = std::strerror(errno);
        return result;
    }
    ::setpgid(pid, pid);

    bool stdoutOpen = true;
    bool stderrOpen = true;
    bool childRunning = true;
    int childStatus = 0;
    const auto deadline = startedAt + request.timeout;
    enum class StopStage { None, Graceful, Term, Kill };
    StopStage stopStage = StopStage::None;
    auto stageDeadline = deadline;
    while (childRunning || stdoutOpen || stderrOpen)
    {
        const auto now = std::chrono::steady_clock::now();
        if (childRunning && request.cancellationRequested && request.cancellationRequested() &&
            stopStage == StopStage::None)
        {
            result.classification = ToolProcessExitClassification::Cancelled;
            stopStage = StopStage::Graceful;
            const auto grace = request.cancellationGraceOverride ?
                request.cancellationGraceOverride() : request.cancellationGrace;
            stageDeadline = now + std::max(grace, std::chrono::milliseconds::zero());
            if (request.requestCancellation) request.requestCancellation();
        }
        else if (childRunning && now >= deadline && stopStage == StopStage::None)
        {
            result.classification = ToolProcessExitClassification::TimedOut;
            stopStage = StopStage::Term;
            ::kill(-pid, SIGTERM);
            result.shutdownClassification = ToolProcessShutdownClassification::Terminated;
            stageDeadline = now + request.terminationGrace;
        }
        else if (childRunning && stopStage == StopStage::Graceful && now >= stageDeadline)
        {
            stopStage = StopStage::Term;
            ::kill(-pid, SIGTERM);
            result.shutdownClassification = ToolProcessShutdownClassification::Terminated;
            stageDeadline = now + request.terminationGrace;
        }
        else if (childRunning && stopStage == StopStage::Term && now >= stageDeadline)
        {
            stopStage = StopStage::Kill;
            ::kill(-pid, SIGKILL);
            result.shutdownClassification = ToolProcessShutdownClassification::Forced;
            stageDeadline = now + request.forceKillGrace;
        }

        std::array<pollfd, 2> descriptors{{
            {stdoutPipe[0], static_cast<short>(POLLIN | POLLHUP), 0},
            {stderrPipe[0], static_cast<short>(POLLIN | POLLHUP), 0}}};
        ::poll(descriptors.data(), descriptors.size(), 25);
        if (stdoutOpen && descriptors[0].revents) Drain(stdoutPipe[0], result.standardOutput, request.outputLimitBytes, stdoutOpen);
        if (stderrOpen && descriptors[1].revents) Drain(stderrPipe[0], result.standardError, request.outputLimitBytes, stderrOpen);
        if (request.onPoll) request.onPoll();

        if (childRunning)
        {
            const auto waited = ::waitpid(pid, &childStatus, WNOHANG);
            if (waited == pid) childRunning = false;
            else if (stopStage == StopStage::Kill &&
                     std::chrono::steady_clock::now() >= stageDeadline)
                ::kill(-pid, SIGKILL);
        }
    }
    if (childRunning) ::waitpid(pid, &childStatus, 0);
    if (result.classification != ToolProcessExitClassification::Cancelled &&
        result.classification != ToolProcessExitClassification::TimedOut)
    {
        if (WIFEXITED(childStatus))
        {
            result.exitCode = WEXITSTATUS(childStatus);
            result.classification = result.exitCode == 0 ?
                ToolProcessExitClassification::Succeeded : ToolProcessExitClassification::Failed;
        }
        else
        {
            result.classification = ToolProcessExitClassification::Crashed;
            result.exitCode = WIFSIGNALED(childStatus) ? 128 + WTERMSIG(childStatus) : -1;
        }
    }
    else
    {
        result.exitCode = WIFEXITED(childStatus) ? WEXITSTATUS(childStatus) : -1;
        if (result.shutdownClassification == ToolProcessShutdownClassification::None)
            result.shutdownClassification = ToolProcessShutdownClassification::Graceful;
        result.diagnosticCode = result.classification == ToolProcessExitClassification::TimedOut ?
            "Editor.Process.TimedOut" : "Editor.Process.Cancelled";
    }
#endif
    result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - startedAt);
    return result;
}

} // namespace SagaEditor
