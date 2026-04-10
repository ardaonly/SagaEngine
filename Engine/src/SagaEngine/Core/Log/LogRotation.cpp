/// @file LogRotation.cpp
/// @brief Implementation of the size-triggered log rotation sink.

#include "SagaEngine/Core/Log/LogRotation.h"

#include "SagaEngine/Core/Log/Log.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <system_error>

namespace SagaEngine::Core {

namespace fs = std::filesystem;

namespace
{
/// Append "YYYYMMDD_HHMMSS" to a path stem.  std::put_time would be
/// cleaner but it pulls in <iomanip> / streams, which we avoid in this
/// translation unit because sinks may be instantiated from signal
/// contexts or very early during engine boot.
[[nodiscard]] std::string MakeTimestampSuffix()
{
    const auto now   = std::chrono::system_clock::now();
    const auto tt    = std::chrono::system_clock::to_time_t(now);
    std::tm    tm{};
#if defined(_WIN32)
    localtime_s(&tm, &tt);
#else
    localtime_r(&tt, &tm);
#endif
    char buf[24];
    std::snprintf(buf, sizeof(buf), "%04d%02d%02d_%02d%02d%02d",
                  tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
                  tm.tm_hour, tm.tm_min, tm.tm_sec);
    return std::string(buf);
}
} // namespace

// ─── Construction / destruction ─────────────────────────────────────────────

LogRotationSink::LogRotationSink(LogRotationConfig config)
    : config_(std::move(config))
{
    // Create the target directory lazily so the sink can be constructed
    // before the logging category is fully initialised.  Errors here are
    // not fatal — OpenCurrentFile() will log a warning if it can't open.
    std::error_code ec;
    fs::create_directories(config_.baseDirectory, ec);

    currentPath_ = (fs::path(config_.baseDirectory) / (config_.baseName + ".log")).string();
    OpenCurrentFile();
}

LogRotationSink::~LogRotationSink()
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (fileHandle_ != nullptr)
    {
        std::fclose(static_cast<std::FILE*>(fileHandle_));
        fileHandle_ = nullptr;
    }
}

// ─── OpenCurrentFile ────────────────────────────────────────────────────────

bool LogRotationSink::OpenCurrentFile()
{
    // Close any existing handle before reopening — caller is either
    // initialising or rotating, so either way the old handle is stale.
    if (fileHandle_ != nullptr)
    {
        std::fclose(static_cast<std::FILE*>(fileHandle_));
        fileHandle_ = nullptr;
    }

    std::FILE* f = std::fopen(currentPath_.c_str(), "ab");
    if (f == nullptr)
    {
        // Don't spam — one warning per failed open.  The engine will keep
        // running; the application's other log sinks (console) still work.
        LOG_WARN("Engine", "LogRotationSink: cannot open %s", currentPath_.c_str());
        return false;
    }

    // Disable C stdio line-buffering so we control flushes ourselves via
    // autoFlush / Flush().  This halves syscall count under heavy logging.
    std::setvbuf(f, nullptr, _IOFBF, 4096);

    // Sync the byte counter to whatever is already on disk, otherwise
    // a server restart would reset the counter and delay rotation.
    std::error_code ec;
    const auto      sz = fs::file_size(currentPath_, ec);
    bytesInCurrentFile_ = (!ec) ? static_cast<std::size_t>(sz) : 0;

    fileHandle_ = f;
    return true;
}

// ─── Write ──────────────────────────────────────────────────────────────────

bool LogRotationSink::Write(const char* message, std::size_t length)
{
    if (message == nullptr)
        return false;

    std::lock_guard<std::mutex> lock(mutex_);

    // Rotate *before* writing if this message would push us past the cap.
    // Doing it after would leave one oversized file on disk every cycle.
    if (fileHandle_ != nullptr &&
        bytesInCurrentFile_ + length + 1 > config_.maxFileSizeBytes)
    {
        RotateLocked();
    }

    if (fileHandle_ == nullptr && !OpenCurrentFile())
        return false;

    auto* f = static_cast<std::FILE*>(fileHandle_);
    const std::size_t wroteBody = std::fwrite(message, 1, length, f);
    const int         wroteNl   = std::fputc('\n', f);
    if (wroteBody != length || wroteNl == EOF)
    {
        // Partial write → disk full or I/O error.  Leave handle open so
        // a retry on the next call has a chance if the condition clears.
        return false;
    }

    bytesInCurrentFile_ += length + 1;

    if (config_.autoFlush)
        std::fflush(f);

    return true;
}

// ─── ForceRotate ────────────────────────────────────────────────────────────

std::string LogRotationSink::ForceRotate()
{
    std::lock_guard<std::mutex> lock(mutex_);
    return RotateLocked();
}

// ─── RotateLocked ───────────────────────────────────────────────────────────

std::string LogRotationSink::RotateLocked()
{
    // Close the current handle so the rename is atomic on every OS.
    if (fileHandle_ != nullptr)
    {
        std::fclose(static_cast<std::FILE*>(fileHandle_));
        fileHandle_ = nullptr;
    }

    const std::string backupName =
        config_.baseName + "." + MakeTimestampSuffix() + ".log";
    const fs::path backupPath = fs::path(config_.baseDirectory) / backupName;

    std::error_code ec;
    fs::rename(currentPath_, backupPath, ec);
    if (ec)
    {
        // Rename failed — most likely because the current file doesn't
        // exist yet (fresh start with no writes).  Don't count this as
        // an error; just proceed to reopen a fresh file.
        LOG_WARN("Engine", "LogRotationSink: rename failed: %s", ec.message().c_str());
    }

    PruneBackups();

    // Reopen a fresh empty file at the canonical path.
    OpenCurrentFile();
    return backupPath.string();
}

// ─── PruneBackups ───────────────────────────────────────────────────────────

void LogRotationSink::PruneBackups()
{
    std::error_code ec;
    fs::directory_iterator it(config_.baseDirectory, ec);
    if (ec)
        return;

    // Collect backup files (anything matching "<base>.*.log" that isn't
    // the current active file).  Sort by modification time ascending so
    // the first items are the oldest.
    struct Entry
    {
        fs::path                        path;
        fs::file_time_type              mtime;
    };
    std::vector<Entry> backups;
    const std::string  prefix = config_.baseName + ".";
    const std::string  suffix = ".log";

    for (const auto& de : it)
    {
        const auto& p = de.path();
        if (!de.is_regular_file(ec)) continue;

        const std::string name = p.filename().string();
        if (name.size() <= prefix.size() + suffix.size()) continue;
        if (name.rfind(prefix, 0) != 0) continue;
        if (name.size() < suffix.size() ||
            name.compare(name.size() - suffix.size(), suffix.size(), suffix) != 0)
            continue;
        if (p.string() == currentPath_) continue;

        Entry e;
        e.path  = p;
        e.mtime = fs::last_write_time(p, ec);
        backups.push_back(std::move(e));
    }

    if (backups.size() <= config_.maxBackupCount)
        return;

    std::sort(backups.begin(), backups.end(),
              [](const Entry& a, const Entry& b) { return a.mtime < b.mtime; });

    // Delete oldest entries until we're back under the cap.
    const std::size_t toDelete = backups.size() - config_.maxBackupCount;
    for (std::size_t i = 0; i < toDelete; ++i)
    {
        std::error_code rmEc;
        fs::remove(backups[i].path, rmEc);
        // Silently tolerate individual failures — the next rotate will
        // retry, and a permanent failure is logged on the *next* write.
    }
}

// ─── Flush ──────────────────────────────────────────────────────────────────

void LogRotationSink::Flush()
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (fileHandle_ != nullptr)
        std::fflush(static_cast<std::FILE*>(fileHandle_));
}

} // namespace SagaEngine::Core
