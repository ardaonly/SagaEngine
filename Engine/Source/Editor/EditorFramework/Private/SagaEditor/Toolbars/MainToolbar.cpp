/// @file MainToolbar.cpp
/// @brief Pimpl scaffolding — auto-generated stub bodies.

#include "SagaEditor/Toolbars/MainToolbar.h"

namespace SagaEditor
{

// ─── Pimpl Definition ────────────────────────────────────────────────────────

struct MainToolbar::Impl {};

// ─── Method Stubs ─────────────────────────────────────────────────────────────

MainToolbar::MainToolbar()
    : m_impl(std::make_unique<Impl>())
{}

MainToolbar::~MainToolbar() = default;

void MainToolbar::AddButton(const std::string& /*id*/, const std::string& /*label*/, std::function<void()> /*onClick*/)
{
    /* stub */
}

void MainToolbar::RemoveButton(const std::string& /*id*/)
{
    /* stub */
}

void MainToolbar::SetButtonEnabled(const std::string& /*id*/, bool /*enabled*/)
{
    /* stub */
}

} // namespace SagaEditor
