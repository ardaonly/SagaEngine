/// @file PlatformFactory.h
/// @brief Abstract factory for all platform-specific object creation.

#pragma once

#include <memory>

namespace Saga {

class IWindow;
class IHighPrecisionTimer;
class IFileSystem;

// ─── Platform Factory Interface ───────────────────────────────────────────────

/// Produces platform-specific implementations of core engine interfaces.
///
/// Usage contract:
///   A concrete subclass (e.g. SDLPlatformFactory) must be registered via
///   PlatformFactory::Set() before Application::Run() is called.
///   The engine never constructs platform objects directly — it always goes
///   through this factory, keeping platform code isolated from engine core.
///
/// Ownership: the engine owns all objects returned by Create* methods.
class PlatformFactory
{
public:
    virtual ~PlatformFactory() = default;

    // ─── Factory Methods ──────────────────────────────────────────────────────

    /// Construct a concrete IWindow for the current platform.
    /// Caller owns the returned object. Init() must still be called by the caller.
    [[nodiscard]] virtual std::unique_ptr<IWindow> CreateWindow() = 0;

    /// Construct a high-precision monotonic timer for the current platform.
    [[nodiscard]] virtual std::unique_ptr<IHighPrecisionTimer> CreateTimer() = 0;

    /// Construct a file system abstraction for the current platform.
    [[nodiscard]] virtual std::unique_ptr<IFileSystem> CreateFileSystem() = 0;

    // ─── Singleton Registry ───────────────────────────────────────────────────

    /// Register the active factory. Must be called exactly once before engine startup.
    /// Does not take ownership — factory lifetime must exceed Application lifetime.
    static void             Set(PlatformFactory* factory) noexcept;

    /// Retrieve the registered factory.
    /// Asserts in debug if Set() was never called.
    [[nodiscard]] static PlatformFactory* Get() noexcept;

private:
    static PlatformFactory* s_Instance; ///< The single registered factory; not owned.
};

} // namespace Saga