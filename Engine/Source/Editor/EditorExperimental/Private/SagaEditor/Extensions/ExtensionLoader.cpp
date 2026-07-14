/// @file ExtensionLoader.cpp
/// @brief Pimpl scaffolding — auto-generated stub bodies.

#include "SagaEditor/Extensions/ExtensionLoader.h"
#include "SagaEditor/Extensions/IEditorExtension.h"
#include "SagaEditor/Extensions/ExtensionManifest.h"

namespace SagaEditor
{

// ─── Pimpl Definition ────────────────────────────────────────────────────────

struct ExtensionLoader::Impl {};

// ─── Method Stubs ─────────────────────────────────────────────────────────────

ExtensionLoader::ExtensionLoader()
    : m_impl(std::make_unique<Impl>())
{}

ExtensionLoader::~ExtensionLoader() = default;

std::unique_ptr<IEditorExtension> ExtensionLoader::Load(const ExtensionManifest& /*manifest*/)
{
    return {};
}

void ExtensionLoader::Unload(const std::string& /*extensionId*/)
{
    /* stub */
}

} // namespace SagaEditor
