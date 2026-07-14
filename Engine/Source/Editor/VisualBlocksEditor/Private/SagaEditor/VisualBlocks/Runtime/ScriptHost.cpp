/// @file ScriptHost.cpp
/// @brief Pimpl scaffolding — auto-generated stub bodies.

#include "SagaEditor/VisualScripting/Runtime/ScriptHost.h"

namespace SagaEditor::VisualScripting
{

// ─── Pimpl Definition ────────────────────────────────────────────────────────

struct ScriptHost::Impl {};

// ─── Method Stubs ─────────────────────────────────────────────────────────────

ScriptHost::ScriptHost()
    : m_impl(std::make_unique<Impl>())
{}

ScriptHost::~ScriptHost() = default;

bool ScriptHost::Initialize(const std::string& /*runtimePath*/)
{
    return false;
}

void ScriptHost::Shutdown()
{
    /* stub */
}

bool ScriptHost::IsReady() const noexcept
{
    return false;
}

} // namespace SagaEditor::VisualScripting
