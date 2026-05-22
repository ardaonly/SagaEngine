/// @file ExternalTool.cpp
/// @brief External process spawning for SagaPipeline.

#include "SagaPipeline/ExternalTool.h"

#include <vector>

#if defined(_WIN32)
    #include <process.h>
#else
    #include <cerrno>
    #include <cstring>
    #include <sys/wait.h>
    #include <unistd.h>
#endif

namespace SagaPipeline
{

ExternalToolResult ProcessExternalToolRunner::Run(
    const ExternalToolInvocation& invocation) const
{
    ExternalToolResult result;
    if (invocation.executable.empty())
    {
        result.exitCode = -1;
        result.error = "empty executable";
        return result;
    }

#if defined(_WIN32)
    std::vector<const char*> argv;
    argv.reserve(invocation.arguments.size() + 2);
    argv.push_back(invocation.executable.c_str());
    for (const std::string& argument : invocation.arguments)
    {
        argv.push_back(argument.c_str());
    }
    argv.push_back(nullptr);

    const intptr_t rc = _spawnvp(_P_WAIT,
                                 invocation.executable.c_str(),
                                 const_cast<char* const*>(argv.data()));
    if (rc == -1)
    {
        result.exitCode = -1;
        result.error = "spawn failed: " + invocation.executable;
        return result;
    }
    result.exitCode = static_cast<int>(rc);
    return result;
#else
    std::vector<char*> argv;
    argv.reserve(invocation.arguments.size() + 2);
    argv.push_back(const_cast<char*>(invocation.executable.c_str()));
    for (const std::string& argument : invocation.arguments)
    {
        argv.push_back(const_cast<char*>(argument.c_str()));
    }
    argv.push_back(nullptr);

    const pid_t pid = fork();
    if (pid < 0)
    {
        result.exitCode = -1;
        result.error = std::string("fork failed: ") + std::strerror(errno);
        return result;
    }

    if (pid == 0)
    {
        execvp(invocation.executable.c_str(), argv.data());
        _exit(127);
    }

    int status = 0;
    if (waitpid(pid, &status, 0) < 0)
    {
        result.exitCode = -1;
        result.error = std::string("waitpid failed: ") + std::strerror(errno);
        return result;
    }

    if (WIFEXITED(status))
    {
        result.exitCode = WEXITSTATUS(status);
        return result;
    }
    if (WIFSIGNALED(status))
    {
        result.exitCode = 128 + WTERMSIG(status);
        return result;
    }

    result.exitCode = -1;
    result.error = "process ended without exit status";
    return result;
#endif
}

} // namespace SagaPipeline
