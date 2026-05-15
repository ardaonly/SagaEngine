/// @file ProcessRunner.cpp
/// @brief POSIX (fork/execvp) and Windows (_spawnvp) implementation of ProcessRunner.

#include "Forge/ProcessRunner.h"

#include <vector>

#include <cstdio>
#include <string>

#if defined(_WIN32)
    #include <process.h>
#else
    #include <sys/wait.h>
    #include <unistd.h>
    #include <cerrno>
    #include <cstring>
#endif

namespace Forge
{

#if defined(_WIN32)

// ─── Windows ──────────────────────────────────────────────────────────────────

int ProcessRunner::Run(const std::string&              executable,
                       const std::vector<std::string>& args,
                       std::string*                    outError) noexcept
{
    std::vector<const char*> argv;
    argv.reserve(args.size() + 2);
    argv.push_back(executable.c_str());
    for (const auto& a : args) argv.push_back(a.c_str());
    argv.push_back(nullptr);

    const intptr_t rc = _spawnvp(_P_WAIT, executable.c_str(),
                                 const_cast<char* const*>(argv.data()));
    if (rc == -1)
    {
        if (outError) *outError = "spawn failed: " + executable;
        return -1;
    }
    return static_cast<int>(rc);
}

#else

// ─── POSIX ────────────────────────────────────────────────────────────────────

int ProcessRunner::Run(const std::string&              executable,
                       const std::vector<std::string>& args,
                       std::string*                    outError) noexcept
{
    std::vector<char*> argv;
    argv.reserve(args.size() + 2);
    argv.push_back(const_cast<char*>(executable.c_str()));
    for (const auto& a : args) argv.push_back(const_cast<char*>(a.c_str()));
    argv.push_back(nullptr);

    const pid_t pid = fork();
    if (pid < 0)
    {
        if (outError) *outError = std::string("fork failed: ") + std::strerror(errno);
        return -1;
    }
    if (pid == 0)
    {
        execvp(executable.c_str(), argv.data());
        _exit(127); // execvp only returns on failure
    }

    int status = 0;
    if (waitpid(pid, &status, 0) < 0)
    {
        if (outError) *outError = std::string("waitpid failed: ") + std::strerror(errno);
        return -1;
    }
    if (WIFEXITED(status))   return WEXITSTATUS(status);
    if (WIFSIGNALED(status)) return 128 + WTERMSIG(status);
    return -1;
}

#endif

// ─── Capture ──────────────────────────────────────────────────────────────────

std::string ProcessRunner::Capture(const std::string&              executable,
                                    const std::vector<std::string>& args) noexcept
{
    // Build a single command string for popen. This is intentionally limited to
    // version-detection use (safe argument values, no user input).
    std::string cmd = executable;
    for (const auto& a : args) { cmd += ' '; cmd += a; }

#if defined(_WIN32)
    cmd += " 2>nul";
    FILE* pipe = _popen(cmd.c_str(), "r");
    auto  done = [](FILE* p) { _pclose(p); };
#else
    cmd += " 2>/dev/null";
    FILE* pipe = popen(cmd.c_str(), "r");
    auto  done = [](FILE* p) { pclose(p); };
#endif

    if (!pipe) return {};
    std::string out;
    char buf[256];
    while (std::fgets(buf, sizeof(buf), pipe)) out += buf;
    done(pipe);
    return out;
}

} // namespace Forge
