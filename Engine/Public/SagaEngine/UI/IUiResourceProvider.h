/// @file IUiResourceProvider.h
/// @brief Logical UI resource provider contract.

#pragma once

#include "SagaEngine/UI/UiResourceIds.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>

namespace SagaEngine::UI
{

/// Result for explicit UI mount registration.
enum class UiResourceMountStatus : std::uint8_t
{
    Ok = 0,
    InvalidPackageId,
    InvalidRoot,
    DuplicateMount,
};

/// Result for resolving a logical UI id to a backend-loadable file path.
enum class UiResourceResolveStatus : std::uint8_t
{
    Ok = 0,
    InvalidId,
    MountNotFound,
    ResourceNotFound,
};

/// Structured UI mount result.
struct UiResourceMountResult
{
    UiResourceMountStatus status = UiResourceMountStatus::InvalidRoot;
    std::string diagnostic;

    [[nodiscard]] bool Succeeded() const noexcept
    {
        return status == UiResourceMountStatus::Ok;
    }
};

/// Structured UI resolve result.
struct UiResourceResolveResult
{
    UiResourceResolveStatus status = UiResourceResolveStatus::ResourceNotFound;
    std::filesystem::path path;
    std::string diagnostic;

    [[nodiscard]] bool Succeeded() const noexcept
    {
        return status == UiResourceResolveStatus::Ok;
    }
};

/// Resolves logical UI ids to concrete paths for the active backend.
class IUiResourceProvider
{
public:
    virtual ~IUiResourceProvider() = default;

    /// Mount loose content. Documents and styles are resolved below root/UI.
    [[nodiscard]] virtual UiResourceMountResult MountContentRoot(
        std::filesystem::path root) = 0;

    /// Mount one package root. Documents and styles are resolved below root/UI.
    [[nodiscard]] virtual UiResourceMountResult MountPackageRoot(
        std::string packageId,
        std::filesystem::path root) = 0;

    /// Resolve one document id to a backend-loadable RML path.
    [[nodiscard]] virtual UiResourceResolveResult ResolveDocument(
        const UiDocumentId& id) const = 0;

    /// Resolve one style id to a backend-loadable RCSS path.
    [[nodiscard]] virtual UiResourceResolveResult ResolveStyle(
        const UiStyleId& id) const = 0;

    /// Number of mounted loose content roots.
    [[nodiscard]] virtual std::size_t ContentMountCount() const noexcept = 0;

    /// Number of mounted package roots.
    [[nodiscard]] virtual std::size_t PackageMountCount() const noexcept = 0;
};

/// Create the default filesystem-backed UI resource provider.
[[nodiscard]] std::unique_ptr<IUiResourceProvider>
CreateDefaultUiResourceProvider();

} // namespace SagaEngine::UI
