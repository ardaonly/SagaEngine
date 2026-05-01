/// @file ExtensionCommandBridge.cpp
/// @brief Pimpl scaffolding — auto-generated stub bodies.

#include "SagaEditor/Extensions/ExtensionCommandBridge.h"

namespace SagaEditor
{

// ─── Pimpl Definition ────────────────────────────────────────────────────────

struct ExtensionCommandBridge::Impl {};

// ─── Method Stubs ─────────────────────────────────────────────────────────────

void ExtensionCommandBridge::Register(IExtensionCommand* /*cmd*/)
{
    /* stub */
}

void ExtensionCommandBridge::Unregister(const std::string& /*commandId*/)
{
    /* stub */
}

IExtensionCommand* ExtensionCommandBridge::Find(const std::string& /*commandId*/) const noexcept
{
    return nullptr;
}

} // namespace SagaEditor
