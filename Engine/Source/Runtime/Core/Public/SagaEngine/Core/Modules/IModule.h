/// @file IModule.h
/// @brief Abstract interface for statically registered engine modules.
///
/// Layer  : SagaEngine / Core / Modules
/// Purpose: Defines the lifecycle and dependency contract for modules that
///          are registered by the executable and managed through a bounded
///          initialization, tick, and shutdown lifecycle.
///
/// Design rules:
///   - Modules are reference-counted (shared_ptr ownership by ModuleManager)
///   - Lifecycle: Unloaded → Loading → Initialized → Active → Unloaded
///   - Dependencies are declared statically; circular deps are detected at load time
///   - A module must be fully initialized before any dependent module starts

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
    Loading,        ///< Instance being created; dependencies being resolved.
    Initialized,    ///< Init() called; resources allocated; not yet ticking.
    Active,         ///< Module is running; Tick() is being called.
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
    bool            required        = true;       ///< If false, engine continues without it.
};

// ─── Module interface ─────────────────────────────────────────────────────

/// Base class for all engine modules. Implement this interface in a statically
/// linked owner and register its factory before ModuleManager initialization.
///
/// Lifecycle:
///   1. ModuleManager creates the module via factory
///   2. ResolveDependencies() is called with pointers to dependency modules
///   3. Init() is called once
///   4. Tick() is called every frame while state is Active
///   5. Shutdown() is called before destruction
///   6. Destructor runs last
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

/// Factory signature used by the static registry.
using CreateModuleFn = IModule* (*)();

} // namespace SagaEngine::Core::Modules
