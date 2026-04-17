/// @file IModule.h
/// @brief Abstract interface for hot-pluggable engine modules.
///
/// Layer  : SagaEngine / Core / Modules
/// Purpose: Defines the lifecycle and dependency contract for modules that
///          can be loaded, unloaded, and reloaded at runtime without
///          restarting the engine.  Modules are the unit of hot-pluggability
///          for subsystems like networking, physics, audio, and rendering.
///
/// Design rules:
///   - Modules are reference-counted (shared_ptr ownership by ModuleManager)
///   - Lifecycle: Unloaded → Loading → Initialized → Active → Suspended → Unloaded
///   - Dependencies are declared statically; circular deps are detected at load time
///   - Module code lives in shared libraries (.dll/.so) or static linkage
///   - A module must be fully initialized before any dependent module starts
///   - Hot-reload suspends the module, unloads the library, loads the new
///     version, reinitializes with the previous state, and resumes

#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace SagaEngine::Core::Modules {

// ─── Module lifecycle states ──────────────────────────────────────────────

enum class ModuleState : uint8_t
{
    Unloaded,       ///< Module code not loaded; no resources held.
    Loading,        ///< Shared library being loaded; dependencies being resolved.
    Initialized,    ///< Init() called; resources allocated; not yet ticking.
    Active,         ///< Module is running; Tick() is being called.
    Suspended,      ///< Module paused for hot-reload; state preserved.
    Failed,         ///< Initialization or runtime error; requires unload.
};

/// Human-readable state name.
[[nodiscard]] inline const char* ModuleStateToString(ModuleState state) noexcept
{
    switch (state)
    {
        case ModuleState::Unloaded:     return "Unloaded";
        case ModuleState::Loading:      return "Loading";
        case ModuleState::Initialized:  return "Initialized";
        case ModuleState::Active:       return "Active";
        case ModuleState::Suspended:    return "Suspended";
        case ModuleState::Failed:       return "Failed";
    }
    return "Unknown";
}

// ─── Module metadata ──────────────────────────────────────────────────────

/// Static metadata describing a module before it is loaded.
struct ModuleDescriptor
{
    const char*     name            = nullptr;    ///< Unique module identifier.
    const char*     version         = "0.0.1";    ///< Semantic version string.
    const char*     author          = nullptr;    ///< Author / team.
    uint32_t        apiVersion      = 1;          ///< Module ABI version.
    bool            hotReloadable  = false;       ///< Supports suspend/reload/resume.
    bool            required        = true;       ///< If false, engine continues without it.
};

// ─── Module interface ─────────────────────────────────────────────────────

/// Base class for all engine modules.  Implement this interface in your
/// module's shared library and export a CreateModule() factory function.
///
/// Lifecycle:
///   1. ModuleManager creates the module via factory
///   2. ResolveDependencies() is called with pointers to dependency modules
///   3. Init() is called once
///   4. Tick() is called every frame while state is Active
///   5. On hot-reload: Suspend() → Unload() → Load() → Init() → Resume()
///   6. Shutdown() is called before destruction
///   7. Destructor runs last
///
/// Thread model:
///   All methods are called from the main engine thread unless the
///   module explicitly creates its own threads internally.
class IModule
{
public:
    virtual ~IModule() = default;

    // ─── Metadata ─────────────────────────────────────────────────────

    /// Return static descriptor for this module.
    [[nodiscard]] virtual ModuleDescriptor GetDescriptor() const noexcept = 0;

    // ─── Dependencies ─────────────────────────────────────────────────

    /// Return list of module names this module depends on.
    /// Called before ResolveDependencies() so the manager can order loads.
    [[nodiscard]] virtual std::vector<std::string> GetDependencies() const noexcept = 0;

    /// Called after all dependencies are loaded.  The manager passes
    /// resolved pointers to each dependency in the same order as
    /// GetDependencies() returned.
    /// @param dependencies  Pointers to dependency modules (same order as GetDependencies).
    /// @return true if all dependencies are valid; false to abort loading.
    virtual bool ResolveDependencies(const std::vector<IModule*>& dependencies) noexcept = 0;

    // ─── Lifecycle ────────────────────────────────────────────────────

    /// One-time initialization.  Called after dependencies are resolved.
    /// @param config  Module-specific configuration blob (JSON or binary).
    /// @return true on success; false transitions module to Failed state.
    virtual bool Init(const void* config, uint32_t configSize) noexcept = 0;

    /// Called every engine tick while the module is in Active state.
    /// @param deltaTime  Time since last tick in seconds.
    virtual void Tick(float deltaTime) noexcept = 0;

    /// Suspend the module for hot-reload.  Pause all background threads,
    /// flush pending work, and serialize internal state to the provided
    /// buffer so the new module version can restore it.
    /// @param outStateBuffer  Output: serialized state bytes.
    /// @param outStateSize    Output: number of bytes written.
    /// @return true if state was successfully serialized.
    virtual bool Suspend(void* outStateBuffer, uint32_t outStateBufferSize,
                         uint32_t& outStateSize) noexcept = 0;

    /// Resume the module after hot-reload.  Restore the serialized state
    /// from the old module version and resume normal operation.
    /// @param stateBuffer  Serialized state from the previous module version.
    /// @param stateSize    Number of bytes in the state buffer.
    /// @return true if state was successfully restored.
    virtual bool Resume(const void* stateBuffer, uint32_t stateSize) noexcept = 0;

    /// One-time cleanup.  Called before the module is unloaded.
    virtual void Shutdown() noexcept = 0;

    // ─── Queries ──────────────────────────────────────────────────────

    /// Current lifecycle state.
    [[nodiscard]] virtual ModuleState GetState() const noexcept = 0;

    /// Human-readable health status (for diagnostics HUD).
    [[nodiscard]] virtual std::string GetHealthStatus() const noexcept = 0;

    /// Memory usage in bytes (for profiling and memory budgets).
    [[nodiscard]] virtual uint64_t GetMemoryUsageBytes() const noexcept = 0;
};

// ─── Factory function signature ─────────────────────────────────────────

/// Shared libraries must export this function to create a module instance.
///
/// extern "C" SAGA_MODULE_EXPORT IModule* CreateModule()
/// {
///     return new MyModule();
/// }
using CreateModuleFn = IModule* (*)();

} // namespace SagaEngine::Core::Modules

/// Platform-specific export macro for module factories.
/// Usage: SAGA_MODULE_EXPORT_IMPL(MyModule) in the module's .cpp file.
#if defined(_WIN32) || defined(_WIN64)
    #define SAGA_MODULE_EXPORT __declspec(dllexport)
#else
    #define SAGA_MODULE_EXPORT __attribute__((visibility("default")))
#endif

#define SAGA_MODULE_EXPORT_IMPL(ModuleClass) \
    extern "C" SAGA_MODULE_EXPORT ::SagaEngine::Core::Modules::IModule* CreateModule() \
    { \
        return new ModuleClass(); \
    }
