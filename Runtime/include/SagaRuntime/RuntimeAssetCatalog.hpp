/// @file RuntimeAssetCatalog.hpp
/// @brief Runtime-owned read-only asset registry access facade.

#pragma once

#include <SagaEngine/Resources/AssetRegistry.h>

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

namespace SagaRuntime
{

/// Read-only Runtime access boundary over a caller-owned asset registry.
///
/// The catalog does not own or mutate the registry. It is the narrow Runtime
/// contract intended for future runtime/client systems that need package asset
/// lookup without receiving raw AssetRegistry ownership.
class RuntimeAssetCatalog
{
public:
    explicit RuntimeAssetCatalog(
        const SagaEngine::Resources::AssetRegistry& registry) noexcept;

    [[nodiscard]] bool IsEmpty() const noexcept;
    [[nodiscard]] std::size_t GetAssetCount() const noexcept;

    [[nodiscard]] bool Contains(
        SagaEngine::Resources::AssetId assetId) const noexcept;
    [[nodiscard]] bool Contains(std::string_view assetKey) const noexcept;

    [[nodiscard]] const SagaEngine::Resources::AssetRegistryEntry* TryGetById(
        SagaEngine::Resources::AssetId assetId) const noexcept;
    [[nodiscard]] const SagaEngine::Resources::AssetRegistryEntry* TryGetByKey(
        std::string_view assetKey) const noexcept;

    [[nodiscard]] std::vector<const SagaEngine::Resources::AssetRegistryEntry*>
    FindByKind(SagaEngine::Resources::AssetKind kind) const;

    [[nodiscard]] std::vector<const SagaEngine::Resources::AssetRegistryEntry*>
    GetPrefetchCandidates() const;

    [[nodiscard]] std::uint64_t GetTotalDiskSizeBytes() const noexcept;

private:
    const SagaEngine::Resources::AssetRegistry* registry_ = nullptr;
};

} // namespace SagaRuntime
