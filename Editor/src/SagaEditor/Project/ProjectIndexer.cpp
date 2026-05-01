/// @file ProjectIndexer.cpp
/// @brief Pimpl scaffolding — auto-generated stub bodies.

#include "SagaEditor/Project/ProjectIndexer.h"

namespace SagaEditor
{

// ─── Pimpl Definition ────────────────────────────────────────────────────────

struct ProjectIndexer::Impl {};

// ─── Method Stubs ─────────────────────────────────────────────────────────────

ProjectIndexer::ProjectIndexer()
    : m_impl(std::make_unique<Impl>())
{}

ProjectIndexer::~ProjectIndexer() = default;

void ProjectIndexer::Index(const std::string& /*rootPath*/)
{
    /* stub */
}

void ProjectIndexer::Reindex()
{
    /* stub */
}

std::vector<AssetEntry> ProjectIndexer::FindByType(const std::string& /*type*/) const
{
    return {};
}

std::vector<AssetEntry> ProjectIndexer::Search(const std::string& /*query*/) const
{
    return {};
}

void ProjectIndexer::SetOnIndexed(std::function<void()> /*cb*/)
{
    /* stub */
}

} // namespace SagaEditor
