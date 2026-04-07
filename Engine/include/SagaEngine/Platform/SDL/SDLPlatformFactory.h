/// @file SDLPlatformFactory.h
/// @brief SDL2-backed concrete implementation of PlatformFactory.

#pragma once

#include "SagaEngine/Platform/PlatformFactory.h"
#include "SagaEngine/Platform/IFileSystem.h"
#include "SagaEngine/Platform/IHighPrecisionTimer.h"
#include "SDLWindow.h"
#include <memory>

namespace Saga {

// ─── SDL Platform Factory ─────────────────────────────────────────────────────

/// Wires PlatformFactory::Create* calls to SDL2 implementations.
///
/// Registration:
///   Instantiate once at the entry point (static lifetime is sufficient) and
///   pass to PlatformFactory::Set() before constructing the Application.
///
///   static SDLPlatformFactory sdlFactory;
///   PlatformFactory::Set(&sdlFactory);
///
/// Forward-compatibility note:
///   CreateTimer and CreateFileSystem return nullptr until their implementations
///   are ready; callers are expected to null-check these returns.
class SDLPlatformFactory final : public PlatformFactory
{
public:
    // ─── Factory Methods ──────────────────────────────────────────────────────

    /// Construct an SDLWindow. Init() must be called by the owner before use.
    [[nodiscard]] std::unique_ptr<IWindow> CreateWindow() override
    {
        return std::make_unique<SDLWindow>();
    }

    /// Not yet implemented — returns nullptr.
    [[nodiscard]] std::unique_ptr<IHighPrecisionTimer> CreateTimer() override
    {
        return nullptr;
    }

    /// Not yet implemented — returns nullptr.
    [[nodiscard]] std::unique_ptr<IFileSystem> CreateFileSystem() override
    {
        return nullptr;
    }
};

} // namespace Saga