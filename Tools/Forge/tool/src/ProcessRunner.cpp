/// @file ProcessRunner.cpp
/// @brief POSIX (fork/execvp) and Windows (_spawnvp) implementation of ProcessRunner.

#include "Forge/ProcessRunner.h"

#include <vector>

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

} // namespace Forge
