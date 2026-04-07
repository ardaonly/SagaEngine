/// @file PlatformFactory.cpp
/// @brief Singleton registry implementation for PlatformFactory.

#include "SagaEngine/Platform/PlatformFactory.h"
#include <cassert>

namespace Saga {

// ─── Static Storage ───────────────────────────────────────────────────────────

PlatformFactory* PlatformFactory::s_Instance = nullptr;

// ─── Registry ─────────────────────────────────────────────────────────────────

void PlatformFactory::Set(PlatformFactory* factory) noexcept
{
    assert(factory && "PlatformFactory::Set() called with null — provide a concrete factory.");
    s_Instance = factory;
}

PlatformFactory* PlatformFactory::Get() noexcept
{
    assert(s_Instance &&
        "PlatformFactory::Get() called before Set() — "
        "register a concrete factory before Application::Run().");
    return s_Instance;
}

} // namespace Saga