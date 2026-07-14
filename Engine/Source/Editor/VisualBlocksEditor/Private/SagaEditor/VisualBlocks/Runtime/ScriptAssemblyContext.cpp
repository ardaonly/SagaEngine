/// @file ScriptAssemblyContext.cpp
/// @brief Pimpl scaffolding — auto-generated stub bodies.

#include "SagaEditor/VisualScripting/Runtime/ScriptAssemblyContext.h"

namespace SagaEditor::VisualScripting
{

// ─── Pimpl Definition ────────────────────────────────────────────────────────

struct ScriptAssemblyContext::Impl {};

// ─── Method Stubs ─────────────────────────────────────────────────────────────

ScriptAssemblyContext::ScriptAssemblyContext()
    : m_impl(std::make_unique<Impl>())
{}

ScriptAssemblyContext::~ScriptAssemblyContext() = default;

bool ScriptAssemblyContext::LoadAssembly(const std::string& /*path*/)
{
    return false;
}

void ScriptAssemblyContext::Unload()
{
    /* stub */
}

bool ScriptAssemblyContext::IsLoaded() const noexcept
{
    return false;
}

} // namespace SagaEditor::VisualScripting
