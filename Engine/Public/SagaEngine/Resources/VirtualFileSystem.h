/// @file VirtualFileSystem.h
/// @brief Runtime virtual content path boundary and basic backends.

#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace SagaEngine::Resources {

/// Read status for virtual file access.
enum class VirtualFileSystemStatus : std::uint8_t
{
    Ok = 0,
    InvalidPath,
    MountNotFound,
    AssetNotFound,
    BackendError
};

/// Mount-table mutation status for virtual file system setup.
enum class VirtualFileSystemMountStatus : std::uint8_t
{
    Ok = 0,
    InvalidMountPoint,
    InvalidBackend,
    DuplicateMount
};

/// Test/backend population status for in-memory file writes.
enum class VirtualFileSystemWriteStatus : std::uint8_t
{
    Ok = 0,
    InvalidPath
};

/// Result of reading one virtual file.
struct VirtualFileSystemReadResult
{
    VirtualFileSystemStatus status = VirtualFileSystemStatus::BackendError;
    std::vector<std::uint8_t> bytes; ///< File payload on success.
    std::string diagnostic;          ///< Human-readable deterministic failure context.

    /// Return true when the read completed successfully.
    [[nodiscard]] bool Succeeded() const noexcept
    {
        return status == VirtualFileSystemStatus::Ok;
    }
};

/// Result of mounting one backend.
struct VirtualFileSystemMountResult
{
    VirtualFileSystemMountStatus status =
        VirtualFileSystemMountStatus::InvalidMountPoint;
    std::string diagnostic; ///< Human-readable deterministic failure context.

    /// Return true when the backend was mounted.
    [[nodiscard]] bool Succeeded() const noexcept
    {
        return status == VirtualFileSystemMountStatus::Ok;
    }
};

/// Result of adding or replacing one in-memory file.
struct VirtualFileSystemWriteResult
{
    VirtualFileSystemWriteStatus status =
        VirtualFileSystemWriteStatus::InvalidPath;
    std::string diagnostic; ///< Human-readable deterministic failure context.

    /// Return true when the file was stored.
    [[nodiscard]] bool Succeeded() const noexcept
    {
        return status == VirtualFileSystemWriteStatus::Ok;
    }
};

/// Mounted storage backend that receives normalized mount-relative paths.
class IVirtualFileSystemBackend
{
public:
    virtual ~IVirtualFileSystemBackend() = default;

    /// Read one mount-relative file path. Paths never start with '/'.
    [[nodiscard]] virtual VirtualFileSystemReadResult ReadFile(
        std::string_view relativePath) const = 0;
};

/// Runtime virtual content access boundary.
class IVirtualFileSystem
{
public:
    virtual ~IVirtualFileSystem() = default;

    /// Read one normalized absolute virtual path.
    [[nodiscard]] virtual VirtualFileSystemReadResult ReadFile(
        std::string_view virtualPath) const = 0;
};

/// Mount-table based virtual file system implementation.
class VirtualFileSystem final : public IVirtualFileSystem
{
public:
    VirtualFileSystem() = default;
    ~VirtualFileSystem() override = default;

    VirtualFileSystem(const VirtualFileSystem&) = delete;
    VirtualFileSystem& operator=(const VirtualFileSystem&) = delete;
    VirtualFileSystem(VirtualFileSystem&&) noexcept = default;
    VirtualFileSystem& operator=(VirtualFileSystem&&) noexcept = default;

    /// Mount a backend at an absolute virtual path prefix.
    [[nodiscard]] VirtualFileSystemMountResult Mount(
        std::string_view mountPoint,
        std::shared_ptr<IVirtualFileSystemBackend> backend);

    /// Read one absolute virtual path through the longest matching mount.
    [[nodiscard]] VirtualFileSystemReadResult ReadFile(
        std::string_view virtualPath) const override;

    /// Number of mounted backends.
    [[nodiscard]] std::size_t MountCount() const noexcept
    {
        return mounts_.size();
    }

private:
    struct MountEntry
    {
        std::string mountPoint;
        std::shared_ptr<IVirtualFileSystemBackend> backend;
    };

    std::vector<MountEntry> mounts_;
};

/// Native-directory backend for runtime loose-file access.
class NativeDirectoryVirtualFileSystemBackend final
    : public IVirtualFileSystemBackend
{
public:
    /// Create a backend rooted at a native filesystem directory.
    explicit NativeDirectoryVirtualFileSystemBackend(
        std::filesystem::path rootDirectory);

    /// Read one mount-relative file from the native directory root.
    [[nodiscard]] VirtualFileSystemReadResult ReadFile(
        std::string_view relativePath) const override;

private:
    std::filesystem::path rootDirectory_;
};

/// In-memory backend for deterministic VFS tests.
class MemoryVirtualFileSystemBackend final : public IVirtualFileSystemBackend
{
public:
    MemoryVirtualFileSystemBackend() = default;
    ~MemoryVirtualFileSystemBackend() override = default;

    /// Add or replace one mount-relative file.
    [[nodiscard]] VirtualFileSystemWriteResult SetFile(
        std::string_view relativePath,
        std::vector<std::uint8_t> bytes);

    /// Read one mount-relative file from memory.
    [[nodiscard]] VirtualFileSystemReadResult ReadFile(
        std::string_view relativePath) const override;

    /// Number of stored files.
    [[nodiscard]] std::size_t FileCount() const noexcept
    {
        return files_.size();
    }

private:
    std::unordered_map<std::string, std::vector<std::uint8_t>> files_;
};

} // namespace SagaEngine::Resources
