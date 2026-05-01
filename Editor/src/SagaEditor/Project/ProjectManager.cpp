/// @file ProjectManager.cpp
/// @brief Pimpl scaffolding — auto-generated stub bodies.

#include "SagaEditor/Project/ProjectManager.h"

namespace SagaEditor
{

// ─── Pimpl Definition ────────────────────────────────────────────────────────

struct ProjectManager::Impl {};

// ─── Method Stubs ─────────────────────────────────────────────────────────────

ProjectManager::ProjectManager()
    : m_impl(std::make_unique<Impl>())
{}

ProjectManager::~ProjectManager() = default;

bool ProjectManager::OpenProject(const std::string& /*path*/)
{
    return false;
}

void ProjectManager::CloseProject()
{
    /* stub */
}

bool ProjectManager::IsProjectOpen() const noexcept
{
    return false;
}

const ProjectInfo& ProjectManager::GetCurrentProject() const
{
    static ProjectInfo s_default; return s_default;
}

void ProjectManager::SetOnProjectChanged(std::function<void()> /*cb*/)
{
    /* stub */
}

} // namespace SagaEditor
