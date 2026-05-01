/// @file ProcessRunner.cpp
/// @brief POSIX (fork/execvp) and Windows (CreateProcessW) implementation.

#include "SagaTools/ProcessRunner.h"

#include <vector>

#if defined(_WIN32)
    #include "SagaTools/Encoding.h"
    #include <windows.h>
    #include <string>
#else
    #include <sys/wait.h>
    #include <unistd.h>
    #include <cerrno>
    #include <cstring>
#endif

namespace SagaTools
{

#if defined(_WIN32)

namespace
{

// ─── Windows command-line quoting ────────────────────────────────────────────

/// Quote a single argument for inclusion in a Windows command line.
///
/// `_spawnvp` and `CreateProcessW` BOTH require us to do this ourselves --
/// neither auto-quotes. The CRT's `parse_cmdline` consumer follows specific
/// rules: backslashes preceding a `"` must be doubled; trailing backslashes
/// inside a quoted region must also be doubled before the closing `"`. Get
/// either rule wrong and `Python.exe path with spaces\script.py arg` becomes
/// `Python.exe`, `path`, `with`, `spaces\script.py`, `arg`.
///
/// Reference: Daniel Colascione's "Everyone quotes command line arguments
/// the wrong way" (the canonical write-up of this trap).
std::wstring QuoteArg(const std::wstring& s)
{
    if (s.empty()) return L"\"\"";

    bool needsQuotes = false;
    for (wchar_t c : s)
    {
        if (c == L' ' || c == L'\t' || c == L'\n' || c == L'\v' || c == L'"')
        {
            needsQuotes = true;
            break;
        }
    }
    if (!needsQuotes) return s;

    std::wstring out;
    out.reserve(s.size() + 2);
    out.push_back(L'"');
    int backslashes = 0;
    for (wchar_t c : s)
    {
        if (c == L'\\')
        {
            ++backslashes;
        }
        else if (c == L'"')
        {
            for (int k = 0; k < backslashes * 2 + 1; ++k) out.push_back(L'\\');
            out.push_back(L'"');
            backslashes = 0;
        }
        else
        {
            for (int k = 0; k < backslashes; ++k) out.push_back(L'\\');
            backslashes = 0;
            out.push_back(c);
        }
    }
    for (int k = 0; k < backslashes * 2; ++k) out.push_back(L'\\');
    out.push_back(L'"');
    return out;
}

} // namespace

// ─── Windows ──────────────────────────────────────────────────────────────────

int ProcessRunner::Run(const std::string&              executable,
                       const std::vector<std::string>& args,
                       std::string*                    outError) noexcept
{
    const std::wstring wexe = Utf8ToWide(executable);
    std::wstring cmdLine = QuoteArg(wexe);
    for (const auto& a : args)
    {
        cmdLine.push_back(L' ');
        cmdLine.append(QuoteArg(Utf8ToWide(a)));
    }

    std::vector<wchar_t> cmdBuffer(cmdLine.begin(), cmdLine.end());
    cmdBuffer.push_back(L'\0');

    STARTUPINFOW         si = {};
    PROCESS_INFORMATION  pi = {};
    si.cb = sizeof(si);

    // lpApplicationName MUST be NULL so CreateProcessW takes the (quoted)
    // first token from lpCommandLine and applies its PATH search:
    // app-dir, CWD, system32, system, Windows, PATH. Setting
    // lpApplicationName to a bare name (e.g. "py") would skip that search
    // and fail with ERROR_FILE_NOT_FOUND (2). Quoting in the command line
    // protects exe paths that contain spaces ("C:\Program Files\...").
    const BOOL ok = CreateProcessW(
        nullptr,            // lpApplicationName -- let Windows resolve via PATH
        cmdBuffer.data(),   // lpCommandLine     -- quoted; first token is exe
        nullptr, nullptr,
        FALSE,
        0,
        nullptr, nullptr,
        &si, &pi);

    if (!ok)
    {
        const DWORD err = GetLastError();
        if (outError)
        {
            *outError = "CreateProcessW failed (" + std::to_string(err)
                      + "): " + executable;
        }
        return -1;
    }

    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD exitCode = 0;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return static_cast<int>(exitCode);
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
        _exit(127);
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

} // namespace SagaTools
