/// @file VirtualFileAssetSource.cpp
/// @brief VFS-backed asset source implementation.

#include "SagaEngine/Resources/VirtualFileAssetSource.h"

#include <utility>

namespace SagaEngine::Resources {

namespace {

[[nodiscard]] std::string AssetContext(
    const StreamingRequest& request,
    const std::string& virtualPath)
{
    std::string context =
        "VirtualFileAssetSource: assetId=" +
        std::to_string(request.assetId);

    context += " kind=" +
               std::to_string(static_cast<int>(request.kind));

    if (!virtualPath.empty())
    {
        context += " path='" + virtualPath + "'";
    }

    return context;
}

} // namespace

// ─── Construction ──────────────────────────────────────────────────────────

VirtualFileAssetSource::VirtualFileAssetSource(
    std::shared_ptr<IVirtualFileSystem> virtualFileSystem,
    VirtualPathResolverFn pathResolver)
    : virtualFileSystem_(std::move(virtualFileSystem))
    , pathResolver_(std::move(pathResolver))
{
}

// ─── IAssetSource ──────────────────────────────────────────────────────────

AssetLoadResult VirtualFileAssetSource::LoadBlocking(
    const StreamingRequest& request,
    const StreamingRequestHandle& handle)
{
    AssetLoadResult result;

    if (handle.CancelRequested())
    {
        result.status = StreamingStatus::Cancelled;
        result.diagnostic = AssetContext(request, {}) + " cancelled before read";
        return result;
    }

    if (!virtualFileSystem_)
    {
        result.status = StreamingStatus::SourceError;
        result.diagnostic = AssetContext(request, {}) + " has null VFS";
        return result;
    }

    if (!pathResolver_)
    {
        result.status = StreamingStatus::SourceError;
        result.diagnostic = AssetContext(request, {}) + " has null path resolver";
        return result;
    }

    const std::string virtualPath =
        pathResolver_(request.assetId, request.kind);
    if (virtualPath.empty())
    {
        result.status = StreamingStatus::AssetNotFound;
        result.diagnostic =
            AssetContext(request, {}) + " resolver returned empty path";
        return result;
    }

    if (handle.CancelRequested())
    {
        result.status = StreamingStatus::Cancelled;
        result.diagnostic =
            AssetContext(request, virtualPath) + " cancelled before VFS read";
        return result;
    }

    VirtualFileSystemReadResult vfsResult =
        virtualFileSystem_->ReadFile(virtualPath);

    if (handle.CancelRequested())
    {
        result.status = StreamingStatus::Cancelled;
        result.diagnostic =
            AssetContext(request, virtualPath) + " cancelled after VFS read";
        return result;
    }

    result.status = MapVfsStatus(vfsResult.status);
    if (result.status != StreamingStatus::Ok)
    {
        result.diagnostic = AssetContext(request, virtualPath);
        if (!vfsResult.diagnostic.empty())
        {
            result.diagnostic += ": " + vfsResult.diagnostic;
        }
        return result;
    }

    result.payload.kind = request.kind;
    result.payload.lod = request.requestedLod;
    result.payload.bytes = std::move(vfsResult.bytes);
    result.payload.sizeBytes =
        static_cast<std::uint64_t>(result.payload.bytes.size());

    return result;
}

StreamingStatus VirtualFileAssetSource::MapVfsStatus(
    VirtualFileSystemStatus status) noexcept
{
    switch (status)
    {
    case VirtualFileSystemStatus::Ok:
        return StreamingStatus::Ok;
    case VirtualFileSystemStatus::AssetNotFound:
    case VirtualFileSystemStatus::MountNotFound:
        return StreamingStatus::AssetNotFound;
    case VirtualFileSystemStatus::InvalidPath:
    case VirtualFileSystemStatus::BackendError:
        return StreamingStatus::SourceError;
    }

    return StreamingStatus::SourceError;
}

} // namespace SagaEngine::Resources
