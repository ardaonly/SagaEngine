/// @file AssetImportManager.cpp
/// @brief Pimpl scaffolding — auto-generated stub bodies.

#include "SagaEditor/Importers/AssetImportManager.h"

namespace SagaEditor
{

// ─── Pimpl Definition ────────────────────────────────────────────────────────

struct AssetImportManager::Impl {};

// ─── Method Stubs ─────────────────────────────────────────────────────────────

AssetImportManager::AssetImportManager()
    : m_impl(std::make_unique<Impl>())
{}

AssetImportManager::~AssetImportManager() = default;

ImportResult AssetImportManager::Import(const ImportJob& /*job*/)
{
    return {};
}

void AssetImportManager::QueueImport(const ImportJob& /*job*/, std::function<void(ImportResult)> /*cb*/)
{
    /* stub */
}

void AssetImportManager::ProcessQueue()
{
    /* stub */
}

} // namespace SagaEditor
