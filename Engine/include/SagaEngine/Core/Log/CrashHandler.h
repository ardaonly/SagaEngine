/// @file CrashHandler.h
/// @brief Crash signal interception and postmortem dump writer.
///
/// Layer  : SagaEngine / Core / Log
/// Purpose: When a production server crashes we have about 100 ms
///          between "something went wrong" and "the kernel tears the
///          process down".  Everything we want to know about the crash
///          — what signal, what address, what thread, what the stack
///          looked like, how far into the frame we were — has to land
///          on disk in that window.  Without it, a crash in the wild
///          is indistinguishable from the machine losing power.
///
///          `CrashHandler` installs async-signal-safe handlers for the
///          signals that actually indicate a real crash (SIGSEGV,
///          SIGILL, SIGFPE, SIGABRT, SIGBUS), captures a minimal
///          snapshot, writes it to a dedicated dump file, and then
///          re-raises the signal to let the kernel do whatever a core
///          dump would have done.  On Windows the path goes through
///          `SetUnhandledExceptionFilter` and writes a `.dmp` via
///          `MiniDumpWriteDump`.
///
/// Design rules:
///   - Async-signal-safe ONLY inside the handler.  No `malloc`, no
///     `printf`, no locks, no C++ streams.  Every buffer and every
///     format string is prepared ahead of time.  The handler talks to
///     the OS via `write(2)` (POSIX) or direct WinAPI (Windows).
///   - Two outputs: a human-readable text summary (thread, signal,
///     instruction pointer, registered frame tag) and, on platforms
///     that support it, a full core/minidump so developers can open
///     the crash in a debugger.
///   - One-shot.  If the handler is invoked re-entrantly we abort
///     immediately — a crash inside the crash handler is pathological
///     and any further attempt to touch state risks corrupting the
///     dump we already wrote.
///
/// Threading:
///   Handlers may fire on any thread.  The engine's "current frame"
///   tag uses a `std::atomic<std::uint64_t>` published by the tick
///   loop; inside the handler we read it with `memory_order_acquire`,
///   which is async-signal-safe on every platform we target.

#pragma once

#include <atomic>
#include <cstdint>
#include <string>

namespace SagaEngine::Core::Log {

// ─── Configuration ──────────────────────────────────────────────────────────

/// Crash handler install options.  Everything here is set once at
/// engine boot; changing the config after install requires a
/// reinstall to take effect.
struct CrashHandlerConfig
{
    /// Directory that will hold crash dumps.  Must exist and be
    /// writable by the server process.  The handler will NOT mkdir —
    /// touching the filesystem allocator inside a signal handler is a
    /// non-starter on POSIX.
    std::string dumpDirectory = "./crashdumps";

    /// Basename stem for dump files.  The final name is
    /// `<stem>.<pid>.<epochSecs>.txt` (and `.dmp` on Windows).  Keeping
    /// pid + time in the filename lets you collect several crashes
    /// from the same host without overwriting.
    std::string fileStem = "saga";

    /// When true, re-raise the signal after writing the dump so the
    /// kernel can still produce a core file and the parent init can
    /// observe a real exit status.  Disable only for test harnesses
    /// that want to continue after capturing a dump.
    bool reRaiseAfterDump = true;

    /// Maximum dump text size in bytes.  The handler uses a static
    /// buffer of this size — larger values waste BSS, smaller values
    /// truncate stack traces in the wild.  8 KiB holds ~60 frames of
    /// symbolised backtrace which is more than enough for postmortem.
    std::uint32_t maxDumpBytes = 8 * 1024;
};

// ─── Frame tagging ──────────────────────────────────────────────────────────

/// The tick loop publishes a small amount of state here so the crash
/// handler can include it in the dump.  All members are atomic because
/// they can be read from any thread and updated from the simulation
/// thread without a lock.
///
/// This is NOT user-facing — it is the crash handler's view of "what
/// was the engine doing when it died".  The fields are deliberately
/// few and cheap to write; anything larger lives in the log queue and
/// will be drained on recovery, not from inside the handler.
class FrameTagProbe
{
public:
    /// Publish the current tick counter.  Called from the simulation
    /// thread at the start of each frame.  Relaxed ordering is
    /// adequate — the handler does not *rely* on this number, it is
    /// purely a hint in the dump text.
    void SetCurrentTick(std::uint64_t tick) noexcept
    {
        currentTick_.store(tick, std::memory_order_relaxed);
    }

    /// Publish the current subsystem name.  The pointer MUST be a
    /// pointer to a string literal or a lifetime-stable static buffer;
    /// the handler will just dereference it with no ownership checks.
    void SetCurrentSubsystem(const char* name) noexcept
    {
        currentSubsystem_.store(name, std::memory_order_release);
    }

    [[nodiscard]] std::uint64_t CurrentTick() const noexcept
    {
        return currentTick_.load(std::memory_order_relaxed);
    }
    [[nodiscard]] const char* CurrentSubsystem() const noexcept
    {
        return currentSubsystem_.load(std::memory_order_acquire);
    }

private:
    std::atomic<std::uint64_t>   currentTick_{0};
    std::atomic<const char*>     currentSubsystem_{nullptr};
};

/// Global probe instance the crash handler reads from.  One per
/// process; defined in `CrashHandler.cpp`.
extern FrameTagProbe g_frameTagProbe;

// ─── Install / uninstall ────────────────────────────────────────────────────

/// Install the signal / SEH handlers.  Call once at engine startup,
/// before any worker thread is spawned, so the handler inherits onto
/// every thread.  Returns `true` on success, `false` if the platform
/// API refused the installation — in which case the engine should log
/// a visible warning and boot anyway; running without crash dumps is
/// degraded but still functional.
[[nodiscard]] bool InstallCrashHandler(const CrashHandlerConfig& config);

/// Remove any installed handlers and reset signal disposition to the
/// OS default.  Safe to call multiple times; safe to call if
/// `InstallCrashHandler` was never called.  Mostly useful for tests.
void UninstallCrashHandler();

/// True between a successful install and the matching uninstall.
/// Diagnostic only — don't gate code on this; a handler that was
/// installed but then silently cleared by another subsystem would
/// lie to the caller.
[[nodiscard]] bool IsCrashHandlerInstalled() noexcept;

} // namespace SagaEngine::Core::Log
