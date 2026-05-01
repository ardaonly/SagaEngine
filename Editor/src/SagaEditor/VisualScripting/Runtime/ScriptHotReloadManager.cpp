/// @file ScriptHotReloadManager.cpp
/// @brief Pimpl scaffolding — auto-generated stub bodies.

#include "SagaEditor/VisualScripting/Runtime/ScriptHotReloadManager.h"

namespace SagaEditor::VisualScripting
{

// ─── Pimpl Definition ────────────────────────────────────────────────────────

struct ScriptHotReloadManager::Impl {};

// ─── Method Stubs ─────────────────────────────────────────────────────────────

ScriptHotReloadManager::ScriptHotReloadManager()
    : m_impl(std::make_unique<Impl>())
{}

ScriptHotReloadManager::~ScriptHotReloadManager() = default;

void ScriptHotReloadManager::Watch(const std::string& /*assemblyPath*/)
{
    /* stub */
}

void ScriptHotReloadManager::Unwatch(const std::string& /*assemblyPath*/)
{
    /* stub */
}

void ScriptHotReloadManager::SetOnReloaded(std::function<void(const std::string& path)> /*cb*/)
{
    /* stub */
}

} // namespace SagaEditor::VisualScripting
