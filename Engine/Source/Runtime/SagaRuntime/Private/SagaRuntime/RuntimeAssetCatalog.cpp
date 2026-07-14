/// @file RuntimeAssetCatalog.cpp
/// @brief Runtime asset catalog facade implementation.

#include <SagaRuntime/RuntimeAssetCatalog.hpp>

namespace SagaRuntime
{

RuntimeAssetCatalog::RuntimeAssetCatalog(
    const SagaEngine::Resources::AssetRegistry& registry) noexcept
    : registry_(&registry)
{
}

bool RuntimeAssetCatalog::IsEmpty() const noexcept
{
    return GetAssetCount() == 0u;
}

std::size_t RuntimeAssetCatalog::GetAssetCount() const noexcept
{
    return registry_->EntryCount();
}

bool RuntimeAssetCatalog::Contains(
    SagaEngine::Resources::AssetId assetId) const noexcept
{
    return TryGetById(assetId) != nullptr;
}

bool RuntimeAssetCatalog::Contains(std::string_view assetKey) const noexcept
{
    return TryGetByKey(assetKey) != nullptr;
}

const SagaEngine::Resources::AssetRegistryEntry*
RuntimeAssetCatalog::TryGetById(
    SagaEngine::Resources::AssetId assetId) const noexcept
{
    return registry_->Find(assetId);
}

const SagaEngine::Resources::AssetRegistryEntry*
RuntimeAssetCatalog::TryGetByKey(std::string_view assetKey) const noexcept
{
    return registry_->FindByKey(assetKey);
}

std::vector<const SagaEngine::Resources::AssetRegistryEntry*>
RuntimeAssetCatalog::FindByKind(
    SagaEngine::Resources::AssetKind kind) const
{
    return registry_->FindByKind(kind);
}

std::vector<const SagaEngine::Resources::AssetRegistryEntry*>
RuntimeAssetCatalog::GetPrefetchCandidates() const
{
    return registry_->GetPrefetchCandidates();
}

std::uint64_t RuntimeAssetCatalog::GetTotalDiskSizeBytes() const noexcept
{
    return registry_->TotalDiskSizeBytes();
}

} // namespace SagaRuntime
