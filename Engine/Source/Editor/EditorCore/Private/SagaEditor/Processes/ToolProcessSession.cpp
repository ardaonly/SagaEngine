// SPDX-License-Identifier: MPL-2.0
#include "SagaEditor/Processes/ToolProcessSession.h"

#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>

#ifdef _WIN32
#include <windows.h>
#else
#include <cerrno>
#include <csignal>
#include <cstring>
#include <pthread.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace SagaEditor
{

class ToolProcessSession::Impl
{
public:
    ~Impl() { Shutdown(std::chrono::milliseconds{500}, std::chrono::milliseconds{500}); }

    bool Start(const ToolProcessSessionOptions& value)
    {
        std::scoped_lock lock(mutex_);
        if (state_ == ToolProcessSessionState::Running || value.process.executable.empty() ||
            value.messageLimitBytes == 0 || value.queueLimitMessages == 0)
        {
            diagnostic_ = "Editor.ProcessSession.InvalidRequest";
            return false;
        }
        options_ = value;
#ifdef _WIN32
        SECURITY_ATTRIBUTES attributes{sizeof(attributes), nullptr, TRUE};
        HANDLE stdinRead = INVALID_HANDLE_VALUE, stdoutWrite = INVALID_HANDLE_VALUE,
               stderrWrite = INVALID_HANDLE_VALUE;
        if (!CreatePipe(&stdinRead, &stdinWrite_, &attributes, 0) ||
            !CreatePipe(&stdoutRead_, &stdoutWrite, &attributes, 0) ||
            !CreatePipe(&stderrRead_, &stderrWrite, &attributes, 0))
        {
            diagnostic_ = "Editor.ProcessSession.PipeFailed"; return false;
        }
        SetHandleInformation(stdinWrite_, HANDLE_FLAG_INHERIT, 0);
        SetHandleInformation(stdoutRead_, HANDLE_FLAG_INHERIT, 0);
        SetHandleInformation(stderrRead_, HANDLE_FLAG_INHERIT, 0);
        std::string command = Quote(value.process.executable.string());
        for (const auto& argument : value.process.arguments) command += " " + Quote(argument);
        std::vector<char> mutableCommand(command.begin(), command.end()); mutableCommand.push_back('\0');
        STARTUPINFOA startup{}; startup.cb = sizeof(startup); startup.dwFlags = STARTF_USESTDHANDLES;
        startup.hStdInput = stdinRead; startup.hStdOutput = stdoutWrite; startup.hStdError = stderrWrite;
        PROCESS_INFORMATION information{};
        const auto cwd = value.process.workingDirectory.string();
        if (!CreateProcessA(nullptr, mutableCommand.data(), nullptr, nullptr, TRUE,
            CREATE_NO_WINDOW | CREATE_NEW_PROCESS_GROUP, nullptr, cwd.empty() ? nullptr : cwd.c_str(),
            &startup, &information))
        {
            diagnostic_ = "Editor.ProcessSession.StartFailed"; return false;
        }
        CloseHandle(stdinRead); CloseHandle(stdoutWrite); CloseHandle(stderrWrite);
        process_ = information.hProcess; threadHandle_ = information.hThread;
        job_ = CreateJobObjectA(nullptr, nullptr);
        if (job_)
        {
            JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
            limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
            SetInformationJobObject(job_, JobObjectExtendedLimitInformation, &limits, sizeof(limits));
            AssignProcessToJobObject(job_, process_);
        }
#else
        int input[2]{-1, -1}, output[2]{-1, -1}, error[2]{-1, -1};
        if (::pipe(input) || ::pipe(output) || ::pipe(error))
        { diagnostic_ = "Editor.ProcessSession.PipeFailed"; return false; }
        pid_ = ::fork();
        if (pid_ == 0)
        {
            ::setpgid(0, 0);
            ::dup2(input[0], STDIN_FILENO); ::dup2(output[1], STDOUT_FILENO); ::dup2(error[1], STDERR_FILENO);
            ::close(input[0]); ::close(input[1]); ::close(output[0]); ::close(output[1]); ::close(error[0]); ::close(error[1]);
            if (!value.process.workingDirectory.empty() && ::chdir(value.process.workingDirectory.c_str())) _exit(126);
            if (!value.process.inheritEnvironment) ::clearenv();
            for (const auto& [key, item] : value.process.environment) ::setenv(key.c_str(), item.c_str(), 1);
            std::vector<std::string> storage{value.process.executable.string()};
            storage.insert(storage.end(), value.process.arguments.begin(), value.process.arguments.end());
            std::vector<char*> argv; for (auto& item : storage) argv.push_back(item.data()); argv.push_back(nullptr);
            ::execvp(argv.front(), argv.data()); _exit(127);
        }
        ::close(input[0]); ::close(output[1]); ::close(error[1]);
        if (pid_ < 0)
        { ::close(input[1]); ::close(output[0]); ::close(error[0]); diagnostic_ = "Editor.ProcessSession.StartFailed"; return false; }
        ::setpgid(pid_, pid_); stdinWrite_ = input[1]; stdoutRead_ = output[0]; stderrRead_ = error[0];
#endif
        state_ = ToolProcessSessionState::Running;
        stdoutThread_ = std::thread([this] { ReadStdout(); });
        stderrThread_ = std::thread([this] { ReadStderr(); });
        waitThread_ = std::thread([this] { WaitForExit(); });
        return true;
    }

    bool WriteLine(const std::string& line)
    {
        if (line.find('\n') != std::string::npos || line.size() > options_.messageLimitBytes) return false;
        std::scoped_lock writeLock(writeMutex_);
        { std::scoped_lock lock(mutex_); if (state_ != ToolProcessSessionState::Running) return false; }
        const std::string framed = line + "\n";
#ifdef _WIN32
        DWORD written = 0;
        return WriteFile(stdinWrite_, framed.data(), static_cast<DWORD>(framed.size()), &written, nullptr) && written == framed.size();
#else
        sigset_t blocked{};
        sigset_t previous{};
        ::sigemptyset(&blocked);
        ::sigaddset(&blocked, SIGPIPE);
        const bool maskChanged = ::pthread_sigmask(SIG_BLOCK, &blocked, &previous) == 0;
        const char* cursor = framed.data(); std::size_t remaining = framed.size();
        bool succeeded = true;
        while (remaining)
        {
            const auto count = ::write(stdinWrite_, cursor, remaining);
            if (count < 0 && errno == EINTR) continue;
            if (count <= 0) { succeeded = false; break; }
            cursor += count; remaining -= static_cast<std::size_t>(count);
        }
        if (!succeeded && errno == EPIPE && maskChanged && ::sigismember(&previous, SIGPIPE) == 0)
        {
            timespec noWait{};
            (void)::sigtimedwait(&blocked, nullptr, &noWait);
        }
        if (maskChanged) (void)::pthread_sigmask(SIG_SETMASK, &previous, nullptr);
        return succeeded;
#endif
    }

    std::optional<std::string> ReadLine(std::chrono::milliseconds timeout)
    {
        std::unique_lock lock(mutex_);
        ready_.wait_for(lock, timeout, [this] { return !lines_.empty() || state_ != ToolProcessSessionState::Running; });
        if (lines_.empty()) return std::nullopt;
        auto line = std::move(lines_.front()); lines_.pop_front(); return line;
    }

    std::string TakeStandardError()
    {
        std::scoped_lock lock(mutex_); auto value = std::move(standardError_); standardError_.clear(); return value;
    }
    ToolProcessSessionState State() const noexcept { std::scoped_lock lock(mutex_); return state_; }
    int ExitCode() const noexcept { std::scoped_lock lock(mutex_); return exitCode_; }
    std::uint64_t ProcessId() const noexcept
    {
        std::scoped_lock lock(mutex_);
#ifdef _WIN32
        return process_ && process_ != INVALID_HANDLE_VALUE ?
            static_cast<std::uint64_t>(::GetProcessId(process_)) : 0;
#else
        return pid_ > 0 ? static_cast<std::uint64_t>(pid_) : 0;
#endif
    }
    std::string Diagnostic() const { std::scoped_lock lock(mutex_); return diagnostic_; }

    void Shutdown(std::chrono::milliseconds graceful, std::chrono::milliseconds terminate)
    {
        bool running;
        { std::scoped_lock lock(mutex_); running = state_ == ToolProcessSessionState::Running ||
            state_ == ToolProcessSessionState::ProtocolLimitExceeded; }
        if (running)
        {
            CloseInput();
            if (!WaitStopped(graceful))
            {
#ifdef _WIN32
                if (job_) TerminateJobObject(job_, 1); else TerminateProcess(process_, 1);
#else
                ::kill(-pid_, SIGTERM);
#endif
                if (!WaitStopped(terminate))
                {
#ifdef _WIN32
                    if (job_) TerminateJobObject(job_, 1); else TerminateProcess(process_, 1);
#else
                    ::kill(-pid_, SIGKILL);
#endif
                }
            }
        }
        CloseInput();
        if (waitThread_.joinable()) waitThread_.join();
        if (stdoutThread_.joinable()) stdoutThread_.join();
        if (stderrThread_.joinable()) stderrThread_.join();
#ifdef _WIN32
        CloseIf(stdoutRead_); CloseIf(stderrRead_); CloseIf(threadHandle_); CloseIf(process_); CloseIf(job_);
#else
        CloseIf(stdoutRead_); CloseIf(stderrRead_);
#endif
    }

private:
    void ReadStdout()
    {
        std::string partial; char buffer[8192];
        for (;;)
        {
#ifdef _WIN32
            DWORD count = 0; if (!ReadFile(stdoutRead_, buffer, sizeof(buffer), &count, nullptr) || count == 0) break;
#else
            const auto count = ::read(stdoutRead_, buffer, sizeof(buffer)); if (count <= 0) break;
#endif
            partial.append(buffer, static_cast<std::size_t>(count));
            if (partial.size() > options_.messageLimitBytes && partial.find('\n') == std::string::npos)
            { LimitExceeded(); break; }
            std::size_t split;
            while ((split = partial.find('\n')) != std::string::npos)
            {
                auto line = partial.substr(0, split); if (!line.empty() && line.back() == '\r') line.pop_back(); partial.erase(0, split + 1);
                if (line.size() > options_.messageLimitBytes) { LimitExceeded(); return; }
                std::scoped_lock lock(mutex_);
                if (lines_.size() >= options_.queueLimitMessages) { diagnostic_ = "Editor.ProcessSession.QueueLimitExceeded"; state_ = ToolProcessSessionState::ProtocolLimitExceeded; ready_.notify_all(); return; }
                lines_.push_back(std::move(line)); ready_.notify_all();
            }
        }
    }
    void ReadStderr()
    {
        char buffer[4096];
        for (;;)
        {
#ifdef _WIN32
            DWORD count = 0; if (!ReadFile(stderrRead_, buffer, sizeof(buffer), &count, nullptr) || count == 0) break;
#else
            const auto count = ::read(stderrRead_, buffer, sizeof(buffer)); if (count <= 0) break;
#endif
            std::scoped_lock lock(mutex_);
            if (standardError_.size() < options_.process.outputLimitBytes)
                standardError_.append(buffer, std::min<std::size_t>(count, options_.process.outputLimitBytes - standardError_.size()));
        }
    }
    void WaitForExit()
    {
        int code = -1; bool crashed = false;
#ifdef _WIN32
        WaitForSingleObject(process_, INFINITE); DWORD result = 1; GetExitCodeProcess(process_, &result); code = static_cast<int>(result); crashed = result != 0;
#else
        int status = 0; while (::waitpid(pid_, &status, 0) < 0 && errno == EINTR) {}
        if (WIFEXITED(status)) code = WEXITSTATUS(status); else { code = WIFSIGNALED(status) ? 128 + WTERMSIG(status) : -1; crashed = true; }
#endif
        std::scoped_lock lock(mutex_); exitCode_ = code;
        if (state_ != ToolProcessSessionState::ProtocolLimitExceeded) state_ = crashed ? ToolProcessSessionState::Crashed : ToolProcessSessionState::Exited;
        ready_.notify_all(); stopped_.notify_all();
    }
    bool WaitStopped(std::chrono::milliseconds timeout)
    {
        std::unique_lock lock(mutex_); return stopped_.wait_for(lock, timeout, [this] { return state_ != ToolProcessSessionState::Running; });
    }
    void LimitExceeded()
    {
        std::scoped_lock lock(mutex_); diagnostic_ = "Editor.ProcessSession.MessageLimitExceeded"; state_ = ToolProcessSessionState::ProtocolLimitExceeded; ready_.notify_all();
    }
    void CloseInput()
    {
        std::scoped_lock lock(writeMutex_);
#ifdef _WIN32
        if (stdinWrite_ != INVALID_HANDLE_VALUE) { CloseHandle(stdinWrite_); stdinWrite_ = INVALID_HANDLE_VALUE; }
#else
        if (stdinWrite_ >= 0) { ::close(stdinWrite_); stdinWrite_ = -1; }
#endif
    }
#ifdef _WIN32
    static std::string Quote(const std::string& value) { if (value.find_first_of(" \t\"") == std::string::npos) return value; return "\"" + value + "\""; }
    static void CloseIf(HANDLE& value) { if (value && value != INVALID_HANDLE_VALUE) { CloseHandle(value); value = INVALID_HANDLE_VALUE; } }
    HANDLE stdinWrite_ = INVALID_HANDLE_VALUE, stdoutRead_ = INVALID_HANDLE_VALUE, stderrRead_ = INVALID_HANDLE_VALUE;
    HANDLE process_ = INVALID_HANDLE_VALUE, threadHandle_ = INVALID_HANDLE_VALUE, job_ = INVALID_HANDLE_VALUE;
#else
    static void CloseIf(int& value) { if (value >= 0) { ::close(value); value = -1; } }
    int stdinWrite_ = -1, stdoutRead_ = -1, stderrRead_ = -1; pid_t pid_ = -1;
#endif
    ToolProcessSessionOptions options_;
    mutable std::mutex mutex_; std::mutex writeMutex_; std::condition_variable ready_, stopped_;
    std::deque<std::string> lines_; std::string standardError_, diagnostic_; int exitCode_ = -1;
    ToolProcessSessionState state_ = ToolProcessSessionState::Stopped;
    std::thread stdoutThread_, stderrThread_, waitThread_;
};

ToolProcessSession::ToolProcessSession() : impl_(std::make_unique<Impl>()) {}
ToolProcessSession::~ToolProcessSession() = default;
bool ToolProcessSession::Start(const ToolProcessSessionOptions& options) { return impl_->Start(options); }
bool ToolProcessSession::WriteLine(const std::string& line) { return impl_->WriteLine(line); }
std::optional<std::string> ToolProcessSession::ReadLine(std::chrono::milliseconds timeout) { return impl_->ReadLine(timeout); }
std::string ToolProcessSession::TakeStandardError() { return impl_->TakeStandardError(); }
ToolProcessSessionState ToolProcessSession::State() const noexcept { return impl_->State(); }
int ToolProcessSession::ExitCode() const noexcept { return impl_->ExitCode(); }
std::uint64_t ToolProcessSession::ProcessId() const noexcept { return impl_->ProcessId(); }
std::string ToolProcessSession::Diagnostic() const { return impl_->Diagnostic(); }
void ToolProcessSession::Shutdown(std::chrono::milliseconds graceful, std::chrono::milliseconds terminate) { impl_->Shutdown(graceful, terminate); }

} // namespace SagaEditor
