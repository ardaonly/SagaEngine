/// @file ContentPipeline.cpp
/// @brief Pimpl scaffolding — auto-generated stub bodies.

#include "SagaEditor/Pipeline/ContentPipeline.h"

namespace SagaEditor
{

// ─── Pimpl Definition ────────────────────────────────────────────────────────

struct ContentPipeline::Impl {};

// ─── Method Stubs ─────────────────────────────────────────────────────────────

ContentPipeline::ContentPipeline()
    : m_impl(std::make_unique<Impl>())
{}

ContentPipeline::~ContentPipeline() = default;

void ContentPipeline::AddSourceDirectory(const std::string& /*path*/)
{
    /* stub */
}

void ContentPipeline::Build()
{
    /* stub */
}

void ContentPipeline::Rebuild()
{
    /* stub */
}

bool ContentPipeline::IsBuilding() const noexcept
{
    return false;
}

} // namespace SagaEditor
