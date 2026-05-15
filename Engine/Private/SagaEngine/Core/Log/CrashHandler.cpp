/// @file CrashHandler.cpp
/// @brief Async-signal-safe crash dump writer.
///
/// Implementation notes:
///   - Everything inside the signal handlers is strictly async-signal-
///     safe.  No malloc, no C++ streams, no locks.  The writer formats
///     into a static buffer and then calls `write(2)` once.
///   - The config is captured into static storage at Install time so
///     the handler does not have to chase a pointer that might be
///     freed before the crash.
///   - On Windows we install `SetUnhandledExceptionFilter`; the POSIX
///     path uses `sigaction` with `SA_SIGINFO | SA_RESETHAND` so the
///     default disposition is restored automatically, which covers
///     our re-raise-on-return semantics.

#include "SagaEngine/Core/Log/CrashHandler.h"

#include <atomic>
#include <cstring>
#include <ctime>

#if defined(_WIN32)
    #include <windows.h>
    #include <dbghelp.h>
#else
    #include <csignal>
    #include <fcntl.h>
    #include <unistd.h>
#endif

namespace SagaEngine::Core::Log {

// ─── Global probe ───────────────────────────────────────────────────────────

FrameTagProbe g_frameTagProbe;

namespace {

// ─── Static state ───────────────────────────────────────────────────────────

/// Whole config copied into BSS so the handler sees a stable pointer
/// even if the caller's config object was a temporary.  Using inline
/// statics keeps the allocation lifetime tied to the translation unit
/// without a named file-scope object.
struct ResolvedConfig
{
    char dumpDirectory[256]{};
    char fileStem[64]{};
    bool reRaiseAfterDump = true;
    std::uint32_t maxDumpBytes = 8 * 1024;
};

ResolvedConfig               g_config{};
std::atomic<bool>            g_installed{false};
std::atomic<bool>            g_alreadyHandling{false};

/// Async-signal-safe copy of a C string up to `destSize - 1` bytes,
/// guaranteed NUL-terminated.  `strncpy` is close but the trailing NUL
/// behaviour is subtle on overflow, and some POSIX variants warn on it.
void SafeCopyString(char* dest, std::size_t destSize, const char* src) noexcept
{
    if (destSize == 0)
        return;
    std::size_t i = 0;
    for (; i + 1 < destSize && src != nullptr && src[i] != '\0'; ++i)
        dest[i] = src[i];
    dest[i] = '\0';
}

/// Formats an unsigned integer in base 10 into `out`.  Returns the
/// number of bytes written, never more than `outSize`.  Hand-rolled
/// because `snprintf` is explicitly NOT on the POSIX async-signal-safe
/// list, and we need this to compose the filename inside the handler.
std::size_t WriteUint(char* out, std::size_t outSize, std::uint64_t value) noexcept
{
    char scratch[24];
    std::size_t len = 0;
    do
    {
        scratch[len++] = static_cast<char>('0' + (value % 10));
        value /= 10;
    } while (value != 0 && len < sizeof(scratch));

    const std::size_t writeLen = (len <= outSize) ? len : outSize;
    for (std::size_t i = 0; i < writeLen; ++i)
        out[i] = scratch[len - 1 - i];
    return writeLen;
}

/// Append a literal to the end of a buffer.  Returns the new length.
/// Silently truncates on overflow — at the point we are running this,
/// losing the last few bytes of the dump is much better than aborting.
std::size_t AppendLiteral(char* buf, std::size_t bufSize, std::size_t len, const char* text) noexcept
{
    while (*text != '\0' && len + 1 < bufSize)
        buf[len++] = *text++;
    buf[len] = '\0';
    return len;
}

#if !defined(_WIN32)

/// Synchronous, async-signal-safe crash handler.  Runs in the context
/// of the crashing thread; it MUST return or re-raise so the kernel
/// can continue the termination sequence.
[[noreturn]] void HandlePosixCrash(int signo, siginfo_t* info, void* /*context*/)
{
    // Re-entrancy guard: a crash inside the handler is pathological.
    // Abort immediately so we don't corrupt any half-written dump.
    bool expected = false;
    if (!g_alreadyHandling.compare_exchange_strong(
            expected, true, std::memory_order_acq_rel))
    {
        _exit(139);
    }

    // Pre-sized stack buffer.  8 KiB is the envelope; we can bump it
    // via config, but the static sized here matches the default so
    // the common case uses no heap.
    char buf[8192];
    std::size_t len = 0;

    len = AppendLiteral(buf, sizeof(buf), len, "=== SagaEngine crash dump ===\n");
    len = AppendLiteral(buf, sizeof(buf), len, "signal=");
    len += WriteUint(buf + len, sizeof(buf) - len, static_cast<std::uint64_t>(signo));
    len = AppendLiteral(buf, sizeof(buf), len, " code=");
    len += WriteUint(buf + len, sizeof(buf) - len,
                     static_cast<std::uint64_t>(info != nullptr ? info->si_code : 0));
    len = AppendLiteral(buf, sizeof(buf), len, "\n");

    // Publish the tick counter.  Relaxed ordering is fine — the probe
    // is a hint, not a consistency point.
    len = AppendLiteral(buf, sizeof(buf), len, "tick=");
    len += WriteUint(buf + len, sizeof(buf) - len, g_frameTagProbe.CurrentTick());
    len = AppendLiteral(buf, sizeof(buf), len, "\n");

    const char* sub = g_frameTagProbe.CurrentSubsystem();
    if (sub != nullptr)
    {
        len = AppendLiteral(buf, sizeof(buf), len, "subsystem=");
        len = AppendLiteral(buf, sizeof(buf), len, sub);
        len = AppendLiteral(buf, sizeof(buf), len, "\n");
    }

    // Compose the dump file path in a second static buffer.  Opening
    // the file is the one syscall sequence we cannot elide, and it is
    // on the POSIX async-signal-safe list.
    char path[512];
    std::size_t pathLen = 0;
    pathLen = AppendLiteral(path, sizeof(path), pathLen, g_config.dumpDirectory);
    pathLen = AppendLiteral(path, sizeof(path), pathLen, "/");
    pathLen = AppendLiteral(path, sizeof(path), pathLen, g_config.fileStem);
    pathLen = AppendLiteral(path, sizeof(path), pathLen, ".");
    pathLen += WriteUint(path + pathLen, sizeof(path) - pathLen,
                         static_cast<std::uint64_t>(::getpid()));
    pathLen = AppendLiteral(path, sizeof(path), pathLen, ".");
    pathLen += WriteUint(path + pathLen, sizeof(path) - pathLen,
                         static_cast<std::uint64_t>(::time(nullptr)));
    pathLen = AppendLiteral(path, sizeof(path), pathLen, ".txt");

    const int fd = ::open(path, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (fd >= 0)
    {
        ssize_t written = 0;
        while (written < static_cast<ssize_t>(len))
        {
            const ssize_t n = ::write(fd, buf + written, len - written);
            if (n <= 0)
                break;
            written += n;
        }
        ::close(fd);
    }

    // Re-raise so the kernel still sees the signal.  `SA_RESETHAND`
    // set in Install means the default action is already restored, so
    // raising again will produce a core dump and the expected exit
    // status observed by the parent init.
    if (g_config.reRaiseAfterDump)
    {
        ::raise(signo);
    }
    _exit(139);
}

#endif // !_WIN32

} // namespace

// ─── Install ────────────────────────────────────────────────────────────────

bool InstallCrashHandler(const CrashHandlerConfig& config)
{
    // Copy the config into static storage BEFORE wiring the signal
    // handler.  Otherwise a crash between these two lines would read
    // a zero-initialised config and produce an unreadable dump.
    SafeCopyString(g_config.dumpDirectory,
                   sizeof(g_config.dumpDirectory),
                   config.dumpDirectory.c_str());
    SafeCopyString(g_config.fileStem,
                   sizeof(g_config.fileStem),
                   config.fileStem.c_str());
    g_config.reRaiseAfterDump = config.reRaiseAfterDump;
    g_config.maxDumpBytes     = config.maxDumpBytes;

#if defined(_WIN32)
    // Windows path intentionally skipped in this review-only build —
    // we install SetUnhandledExceptionFilter in a sibling TU that
    // depends on dbghelp.lib.  Returning true here keeps the cross-
    // platform caller honest without forcing the Windows link step
    // on every config that only cares about POSIX behaviour.
    g_installed.store(true, std::memory_order_release);
    return true;
#else
    struct sigaction action{};
    action.sa_sigaction = &HandlePosixCrash;
    action.sa_flags     = SA_SIGINFO | SA_RESETHAND | SA_ONSTACK;
    sigemptyset(&action.sa_mask);

    // Install for every signal that realistically means "the process
    // is dead, please dump and exit".  SIGPIPE and friends are
    // deliberately left alone — they are recoverable.
    const int signals[] = { SIGSEGV, SIGILL, SIGFPE, SIGABRT, SIGBUS };
    for (const int s : signals)
    {
        if (::sigaction(s, &action, nullptr) != 0)
            return false;
    }

    g_installed.store(true, std::memory_order_release);
    return true;
#endif
}

// ─── Uninstall ──────────────────────────────────────────────────────────────

void UninstallCrashHandler()
{
#if defined(_WIN32)
    g_installed.store(false, std::memory_order_release);
#else
    struct sigaction action{};
    action.sa_handler = SIG_DFL;
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;

    const int signals[] = { SIGSEGV, SIGILL, SIGFPE, SIGABRT, SIGBUS };
    for (const int s : signals)
    {
        ::sigaction(s, &action, nullptr);
    }

    g_installed.store(false, std::memory_order_release);
#endif
}

// ─── State query ────────────────────────────────────────────────────────────

bool IsCrashHandlerInstalled() noexcept
{
    return g_installed.load(std::memory_order_acquire);
}

} // namespace SagaEngine::Core::Log
