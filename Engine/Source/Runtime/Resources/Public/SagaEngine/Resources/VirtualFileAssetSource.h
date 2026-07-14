/// @file VirtualFileAssetSource.h
/// @brief VFS-backed asset source implementation.

#pragma once

#include "SagaEngine/Resources/IAssetSource.h"
#include "SagaEngine/Resources/VirtualFileSystem.h"

#include <functional>
#include <memory>
#include <string>

namespace SagaEngine::Resources {

/// Resolves one asset request to a normalized absolute virtual path.
using VirtualPathResolverFn = std::function<std::string(AssetId, AssetKind)>;

/// Asset source adapter that reads bytes through IVirtualFileSystem.
class VirtualFileAssetSource final : public IAssetSource
{
public:
    /// Build a source around a virtual file system and asset path resolver.
    explicit VirtualFileAssetSource(
        std::shared_ptr<IVirtualFileSystem> virtualFileSystem,
        VirtualPathResolverFn pathResolver);

    ~VirtualFileAssetSource() override = default;

    VirtualFileAssetSource(const VirtualFileAssetSource&) = delete;
    VirtualFileAssetSource& operator=(const VirtualFileAssetSource&) = delete;
    VirtualFileAssetSource(VirtualFileAssetSource&&) = delete;
    VirtualFileAssetSource& operator=(VirtualFileAssetSource&&) = delete;

    /// Load one asset synchronously through the configured VFS.
    [[nodiscard]] AssetLoadResult LoadBlocking(
        const StreamingRequest& request,
        const StreamingRequestHandle& handle) override;

    /// Return a stable name for streaming diagnostics.
    [[nodiscard]] const char* DebugName() const noexcept override
    {
        return "VirtualFileAssetSource";
    }

private:
    [[nodiscard]] static StreamingStatus MapVfsStatus(
        VirtualFileSystemStatus status) noexcept;

    std::shared_ptr<IVirtualFileSystem> virtualFileSystem_;
    VirtualPathResolverFn pathResolver_;
};

} // namespace SagaEngine::Resources
