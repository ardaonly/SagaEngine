/// @file LogRotation.h
/// @brief Size-triggered, time-stamped log file rotation sink.
///
/// Layer  : SagaEngine / Core / Log
/// Purpose: Long-running MMO servers produce gigabytes of log output per
///          day.  A single ever-growing file is impossible to ship to an
///          analyst and eats disk on the host.  Rotation buys us three
///          things at once:
///            1. Bounded disk usage via a maximum retained file count.
///            2. Natural shard points on the time axis for grep / ship.
///            3. A clean "roll over" boundary when an admin issues
///               SIGHUP or the service restarts.
///
/// Design:
///   - The sink writes to `<baseName>.log`.  When it crosses
///     `maxFileSizeBytes`, it renames the current file to
///     `<baseName>.YYYYMMDD_HHMMSS.log` and opens a fresh one.  The old
///     files are kept up to `maxBackupCount`; the oldest are deleted to
///     stay under the cap.
///   - Thread-safe: one mutex guards file IO and rotation bookkeeping.
///     Log messages are rare enough (<100 KiB/s on average) that the
///     mutex is never hot; the lock is released around any filesystem
///     syscall so a slow fsync cannot stall the whole server.
///   - Rotation failures (rename / create errors) fall back to best-
///     effort behaviour: we keep writing to the current handle and log
///     a single warning.  We never crash the server over a log sink.
///
/// Why size-only (not time-based):
///   MMO traffic has huge variance — a quiet morning produces 10 MiB,
///   a raid boss fight produces 2 GiB.  A time-only policy gives you
///   2 GiB files you can't load into a viewer.  Size-triggered rotation
///   keeps every file loadable, and ops teams can wrap it with cron
///   for time-based rotation if they really want both.

#pragma once

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

namespace SagaEngine::Core {

// ─── LogRotationSink ────────────────────────────────────────────────────────

/// Configuration passed at construction time.  All fields have sensible
/// defaults so tests can construct a sink with one line.
struct LogRotationConfig
{
    std::string baseDirectory   = "./logs";   ///< Target directory; created if missing.
    std::string baseName        = "saga";     ///< File name stem, e.g. "saga" -> saga.log.
    std::size_t maxFileSizeBytes = 64 * 1024 * 1024;  ///< 64 MiB rotation threshold.
    std::size_t maxBackupCount   = 10;                ///< Keep at most N rotated files.
    bool        autoFlush        = true;              ///< fflush after every write.
};

/// Append-only log sink with size-based rotation.  Safe to share across
/// threads.  One instance per log stream; typically the engine owns a
/// single sink and all LOG_* macros funnel into it.
class LogRotationSink
{
public:
    explicit LogRotationSink(LogRotationConfig config);
    ~LogRotationSink();

    LogRotationSink(const LogRotationSink&)            = delete;
    LogRotationSink& operator=(const LogRotationSink&) = delete;
    LogRotationSink(LogRotationSink&&)                 = delete;
    LogRotationSink& operator=(LogRotationSink&&)      = delete;

    /// Write one log line.  `message` does not need a trailing newline —
    /// the sink appends `'\n'` itself so every record starts on its own
    /// line regardless of how the caller formatted it.
    ///
    /// Returns false if the write failed (disk full, permission denied).
    /// On failure the sink stays open and will retry on the next call.
    bool Write(const char* message, std::size_t length);

    /// Convenience overload for null-terminated strings.
    bool Write(const std::string& message)
    {
        return Write(message.data(), message.size());
    }

    /// Force an immediate rotation regardless of current file size.
    /// Used by SIGHUP handlers and the admin "/rotatelogs" command.
    /// Returns the name of the new backup file, or empty on failure.
    std::string ForceRotate();

    /// Flush the underlying file handle.  No-op if `autoFlush` is true.
    void Flush();

    // ── Diagnostics ────────────────────────────────────────────────────────

    [[nodiscard]] std::size_t BytesWrittenSinceRotation() const noexcept
    {
        return bytesInCurrentFile_;
    }

    [[nodiscard]] const std::string& CurrentFilePath() const noexcept
    {
        return currentPath_;
    }

private:
    /// Open (or create) the current log file at `currentPath_` and size
    /// the internal byte counter to match the file's existing size so we
    /// don't mistakenly skip rotation on restart.
    bool OpenCurrentFile();

    /// Close the current file handle, rename it with a timestamp suffix,
    /// and prune old backups down to `maxBackupCount`.  Internal — must
    /// be called with `mutex_` held.
    std::string RotateLocked();

    /// Delete oldest backups so at most `maxBackupCount` remain.  Called
    /// after a successful rotation.
    void PruneBackups();

    LogRotationConfig config_;
    std::mutex        mutex_;
    void*             fileHandle_         = nullptr; ///< FILE* kept opaque to stay iosfwd-friendly.
    std::size_t       bytesInCurrentFile_ = 0;
    std::string       currentPath_;
};

} // namespace SagaEngine::Core
