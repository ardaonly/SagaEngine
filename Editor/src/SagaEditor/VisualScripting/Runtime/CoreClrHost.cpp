/// @file CoreClrHost.cpp
/// @brief Pimpl scaffolding — auto-generated stub bodies.

#include "SagaEditor/VisualScripting/Runtime/CoreClrHost.h"

namespace SagaEditor::VisualScripting
{

// ─── Pimpl Definition ────────────────────────────────────────────────────────

struct CoreClrHost::Impl {};

// ─── Method Stubs ─────────────────────────────────────────────────────────────

CoreClrHost::CoreClrHost()
    : m_impl(std::make_unique<Impl>())
{}

CoreClrHost::~CoreClrHost() = default;

bool CoreClrHost::Load(const std::string& /*clrPath*/)
{
    return false;
}

void CoreClrHost::Unload()
{
    /* stub */
}

bool CoreClrHost::IsLoaded() const noexcept
{
    return false;
}

} // namespace SagaEditor::VisualScripting
