/// @file VirtualFileSystem.cpp
/// @brief Runtime virtual content path boundary and basic backends.

#include "SagaEngine/Resources/VirtualFileSystem.h"

#include <algorithm>
#include <fstream>
#include <iterator>
#include <utility>

namespace SagaEngine::Resources {

namespace {

[[nodiscard]] bool ContainsBackslash(std::string_view path) noexcept
{
    return path.find('\\') != std::string_view::npos;
}

[[nodiscard]] bool HasInvalidSegment(std::string_view path) noexcept
{
    std::size_t segmentStart = 0;
    while (segmentStart <= path.size())
    {
        const std::size_t segmentEnd = path.find('/', segmentStart);
        const std::size_t count =
            (segmentEnd == std::string_view::npos)
                ? path.size() - segmentStart
                : segmentEnd - segmentStart;

        const std::string_view segment = path.substr(segmentStart, count);
        if (segment.empty() || segment == "." || segment == "..")
        {
            return true;
        }

        if (segmentEnd == std::string_view::npos)
        {
            break;
        }

        segmentStart = segmentEnd + 1;
    }

    return false;
}

[[nodiscard]] bool IsValidAbsoluteVirtualPath(std::string_view path) noexcept
{
    if (path.empty() || path.front() != '/' || ContainsBackslash(path))
    {
        return false;
    }

    if (path == "/")
    {
        return true;
    }

    return !HasInvalidSegment(path.substr(1));
}

[[nodiscard]] bool IsValidRelativeVirtualPath(std::string_view path) noexcept
{
    if (path.empty() || path.front() == '/' || ContainsBackslash(path))
    {
        return false;
    }

    return !HasInvalidSegment(path);
}

[[nodiscard]] bool MountMatches(
    std::string_view mountPoint,
    std::string_view virtualPath) noexcept
{
    if (mountPoint == "/")
    {
        return true;
    }

    if (virtualPath.size() < mountPoint.size())
    {
        return false;
    }

    if (virtualPath.substr(0, mountPoint.size()) != mountPoint)
    {
        return false;
    }

    return virtualPath.size() == mountPoint.size() ||
           virtualPath[mountPoint.size()] == '/';
}

[[nodiscard]] std::string RelativePathForMount(
    std::string_view mountPoint,
    std::string_view virtualPath)
{
    if (mountPoint == "/")
    {
        return std::string(virtualPath.substr(1));
    }

    if (virtualPath.size() == mountPoint.size())
    {
        return {};
    }

    return std::string(virtualPath.substr(mountPoint.size() + 1));
}

[[nodiscard]] VirtualFileSystemReadResult InvalidPathResult(
    std::string_view path,
    std::string_view context)
{
    VirtualFileSystemReadResult result;
    result.status = VirtualFileSystemStatus::InvalidPath;
    result.diagnostic = "VirtualFileSystem: invalid " + std::string(context) +
                        " path '" + std::string(path) + "'";
    return result;
}

} // namespace

// ─── VirtualFileSystem ─────────────────────────────────────────────────────

VirtualFileSystemMountResult VirtualFileSystem::Mount(
    std::string_view mountPoint,
    std::shared_ptr<IVirtualFileSystemBackend> backend)
{
    VirtualFileSystemMountResult result;

    if (!IsValidAbsoluteVirtualPath(mountPoint))
    {
        result.status = VirtualFileSystemMountStatus::InvalidMountPoint;
        result.diagnostic =
            "VirtualFileSystem: invalid mount point '" +
            std::string(mountPoint) + "'";
        return result;
    }

    if (!backend)
    {
        result.status = VirtualFileSystemMountStatus::InvalidBackend;
        result.diagnostic = "VirtualFileSystem: null backend for mount point '" +
                            std::string(mountPoint) + "'";
        return result;
    }

    const std::string normalizedMountPoint(mountPoint);
    const auto existing = std::find_if(
        mounts_.begin(),
        mounts_.end(),
        [&normalizedMountPoint](const MountEntry& entry) {
            return entry.mountPoint == normalizedMountPoint;
        });

    if (existing != mounts_.end())
    {
        result.status = VirtualFileSystemMountStatus::DuplicateMount;
        result.diagnostic =
            "VirtualFileSystem: duplicate mount point '" +
            normalizedMountPoint + "'";
        return result;
    }

    mounts_.push_back(MountEntry{normalizedMountPoint, std::move(backend)});
    result.status = VirtualFileSystemMountStatus::Ok;
    return result;
}

VirtualFileSystemReadResult VirtualFileSystem::ReadFile(
    std::string_view virtualPath) const
{
    if (!IsValidAbsoluteVirtualPath(virtualPath) || virtualPath == "/")
    {
        return InvalidPathResult(virtualPath, "virtual");
    }

    const MountEntry* bestMount = nullptr;
    for (const MountEntry& entry : mounts_)
    {
        if (!MountMatches(entry.mountPoint, virtualPath))
        {
            continue;
        }

        if (!bestMount || entry.mountPoint.size() > bestMount->mountPoint.size())
        {
            bestMount = &entry;
        }
    }

    if (!bestMount)
    {
        VirtualFileSystemReadResult result;
        result.status = VirtualFileSystemStatus::MountNotFound;
        result.diagnostic =
            "VirtualFileSystem: no mount for virtual path '" +
            std::string(virtualPath) + "'";
        return result;
    }

    const std::string relativePath =
        RelativePathForMount(bestMount->mountPoint, virtualPath);
    if (!IsValidRelativeVirtualPath(relativePath))
    {
        return InvalidPathResult(relativePath, "mount-relative");
    }

    return bestMount->backend->ReadFile(relativePath);
}

// ─── NativeDirectoryVirtualFileSystemBackend ───────────────────────────────

NativeDirectoryVirtualFileSystemBackend::NativeDirectoryVirtualFileSystemBackend(
    std::filesystem::path rootDirectory)
    : rootDirectory_(std::move(rootDirectory))
{
}

VirtualFileSystemReadResult NativeDirectoryVirtualFileSystemBackend::ReadFile(
    std::string_view relativePath) const
{
    if (!IsValidRelativeVirtualPath(relativePath))
    {
        return InvalidPathResult(relativePath, "native-directory relative");
    }

    const std::filesystem::path resolvedPath =
        rootDirectory_ / std::filesystem::path(std::string(relativePath));

    std::ifstream input(resolvedPath, std::ios::binary);
    if (!input)
    {
        VirtualFileSystemReadResult result;
        result.status = VirtualFileSystemStatus::AssetNotFound;
        result.diagnostic =
            "NativeDirectoryVirtualFileSystemBackend: cannot open '" +
            resolvedPath.string() + "'";
        return result;
    }

    VirtualFileSystemReadResult result;
    result.bytes.assign(
        std::istreambuf_iterator<char>(input),
        std::istreambuf_iterator<char>());

    if (input.bad())
    {
        result.bytes.clear();
        result.status = VirtualFileSystemStatus::BackendError;
        result.diagnostic =
            "NativeDirectoryVirtualFileSystemBackend: read failed for '" +
            resolvedPath.string() + "'";
        return result;
    }

    result.status = VirtualFileSystemStatus::Ok;
    return result;
}

// ─── MemoryVirtualFileSystemBackend ────────────────────────────────────────

VirtualFileSystemWriteResult MemoryVirtualFileSystemBackend::SetFile(
    std::string_view relativePath,
    std::vector<std::uint8_t> bytes)
{
    VirtualFileSystemWriteResult result;

    if (!IsValidRelativeVirtualPath(relativePath))
    {
        result.status = VirtualFileSystemWriteStatus::InvalidPath;
        result.diagnostic =
            "MemoryVirtualFileSystemBackend: invalid relative path '" +
            std::string(relativePath) + "'";
        return result;
    }

    files_[std::string(relativePath)] = std::move(bytes);
    result.status = VirtualFileSystemWriteStatus::Ok;
    return result;
}

VirtualFileSystemReadResult MemoryVirtualFileSystemBackend::ReadFile(
    std::string_view relativePath) const
{
    if (!IsValidRelativeVirtualPath(relativePath))
    {
        return InvalidPathResult(relativePath, "memory relative");
    }

    const auto found = files_.find(std::string(relativePath));
    if (found == files_.end())
    {
        VirtualFileSystemReadResult result;
        result.status = VirtualFileSystemStatus::AssetNotFound;
        result.diagnostic =
            "MemoryVirtualFileSystemBackend: asset not found '" +
            std::string(relativePath) + "'";
        return result;
    }

    VirtualFileSystemReadResult result;
    result.status = VirtualFileSystemStatus::Ok;
    result.bytes = found->second;
    return result;
}

} // namespace SagaEngine::Resources
