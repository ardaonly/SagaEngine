/// @file ModuleManager.h
/// @brief Lifecycle manager for hot-pluggable engine modules.
///
/// Layer  : SagaEngine / Core / Modules
/// Purpose: The ModuleManager orchestrates the loading, initialization,
///          ticking, hot-reloading, and unloading of IModule instances.
///          It resolves dependencies, enforces load ordering, and provides
///          runtime health monitoring.
///
/// Design rules:
///   - Manager is owned by the Engine singleton (not the WorldNode)
///   - Modules are loaded in dependency order (topological sort)
///   - Circular dependencies are detected and reported at load time
///   - Hot-reload suspends the old module, loads the new one, transfers state
///   - Failed modules are gracefully unloaded; engine continues if not required
///   - All module operations are synchronous (no background loading threads)

#pragma once

#include "SagaEngine/Core/Modules/IModule.h"
#include "SagaEngine/Core/Modules/ModuleRegistry.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace SagaEngine::Core::Modules {

// ─── Manager configuration ──────────────────────────────────────────────

struct ModuleManagerConfig
{
    const char* pluginsDirectory = "plugins";  ///< Directory for dynamic modules.
    bool        autoLoadPlugins  = true;        ///< Auto-discover and load plugins at Init.
    uint32_t    maxModules       = 64;          ///< Hard cap on loaded modules.
    bool        strictDeps       = true;        ///< Fail load if any dependency is missing.
};

// ─── Module health ──────────────────────────────────────────────────────

struct ModuleHealthInfo
{
    ModuleState state              = ModuleState::Unloaded;
    uint64_t    memoryUsageBytes   = 0;
    uint64_t    totalTickTimeUs    = 0;
    uint64_t    tickCount          = 0;
    float       averageTickMs      = 0.0f;
    std::string healthStatus;
};

// ─── Module event ───────────────────────────────────────────────────────

/// Callback for module lifecycle events.
using ModuleEventCallback = std::function<void(
    const char* moduleName,
    ModuleState oldState,
    ModuleState newState,
    const char* message)>;

// ─── Module manager ─────────────────────────────────────────────────────

/// Orchestrates module lifecycle.
///
/// Usage:
///   1. ModuleManager manager;
///   2. manager.Init(config);
///   3. manager.LoadModule("Networking");  // Dependencies auto-resolved
///   4. manager.Tick(deltaTime);           // Ticks all active modules
///   5. manager.HotReloadModule("Physics"); // Suspend → reload → resume
///   6. manager.Shutdown();                // Unload all modules
///
/// Thread model:
///   All methods are called from the main engine thread.
///   Internal module state is protected by a mutex for safety.
class ModuleManager
{
public:
    ModuleManager() noexcept;
    ~ModuleManager() noexcept;

    ModuleManager(const ModuleManager&)            = delete;
    ModuleManager& operator=(const ModuleManager&) = delete;

    // ─── Initialization ───────────────────────────────────────────────

    /// Initialize the module manager.
    /// @param config  Manager configuration.
    /// @return true on success; false if registry scan failed.
    bool Init(const ModuleManagerConfig& config) noexcept;

    /// Shutdown the manager and unload all modules.
    void Shutdown() noexcept;

    // ─── Module loading ───────────────────────────────────────────────

    /// Load and initialize a module by name.
    /// Dependencies are resolved automatically before Init() is called.
    /// @param name       Module name (must be registered in ModuleRegistry).
    /// @param config     Optional module-specific configuration blob.
    /// @param configSize Size of the configuration blob in bytes.
    /// @return true if the module loaded and initialized successfully.
    bool LoadModule(const char* name, const void* config = nullptr,
                    uint32_t configSize = 0) noexcept;

    /// Unload a module by name.
    /// Calls Shutdown() on the module and releases its resources.
    /// @param name  Module name.
    /// @return true if the module was unloaded successfully.
    bool UnloadModule(const char* name) noexcept;

    // ─── Hot-reload ───────────────────────────────────────────────────

    /// Hot-reload a module without stopping the engine.
    ///
    /// Sequence:
    ///   1. Suspend the old module (serialize state)
    ///   2. Unload the old module's shared library
    ///   3. Load the new module's shared library
    ///   4. Initialize the new module with the old state
    ///   5. Resume the new module (restore state)
    ///
    /// @param name          Module name.
    /// @param newLibraryPath Path to the new shared library (or empty to rescan).
    /// @return true if hot-reload completed successfully.
    bool HotReloadModule(const char* name, const char* newLibraryPath = nullptr) noexcept;

    // ─── Tick ─────────────────────────────────────────────────────────

    /// Tick all active modules.
    /// @param deltaTime  Time since last tick in seconds.
    void Tick(float deltaTime) noexcept;

    // ─── Queries ──────────────────────────────────────────────────────

    /// Get a module instance by name (raw pointer; do not delete).
    [[nodiscard]] IModule* GetModule(const char* name) noexcept;
    [[nodiscard]] const IModule* GetModule(const char* name) const noexcept;

    /// Check if a module is loaded and active.
    [[nodiscard]] bool IsModuleLoaded(const char* name) const noexcept;

    /// Get all loaded module names.
    [[nodiscard]] std::vector<std::string> GetLoadedModuleNames() const noexcept;

    /// Get health info for a loaded module.
    [[nodiscard]] ModuleHealthInfo GetModuleHealth(const char* name) const noexcept;

    /// Get health info for all loaded modules.
    [[nodiscard]] std::unordered_map<std::string, ModuleHealthInfo>
        GetAllModuleHealth() const noexcept;

    // ─── Events ───────────────────────────────────────────────────────

    /// Register a callback for module lifecycle events.
    /// @param callback  Called on state transitions (load, unload, hot-reload, fail).
    void SetEventCallback(ModuleEventCallback callback) noexcept;

    // ─── Diagnostics ──────────────────────────────────────────────────

    /// Total memory usage of all loaded modules in bytes.
    [[nodiscard]] uint64_t GetTotalMemoryUsageBytes() const noexcept;

    /// Number of currently loaded modules.
    [[nodiscard]] uint32_t GetLoadedCount() const noexcept;

private:
    // Internal module bookkeeping.
    struct ModuleInstance
    {
        std::shared_ptr<IModule> instance;
        ModuleState              state         = ModuleState::Unloaded;
        std::string              name;
        uint64_t                 loadOrder     = 0;
        uint64_t                 totalTickTimeUs = 0;
        uint64_t                 tickCount     = 0;
    };

    /// Resolve and load dependencies for a module (topological sort).
    [[nodiscard]] bool ResolveDependencies(const char* name,
                                            std::vector<std::string>& outLoadOrder) noexcept;

    /// Detect circular dependencies using DFS.
    [[nodiscard]] bool HasCircularDependency(const char* name) noexcept;

    /// Internal DFS helper for circular dependency detection.
    [[nodiscard]] bool DFS_Visit(const char* name,
                                  std::unordered_map<std::string, int>& visitState) noexcept;

    ModuleManagerConfig m_config;
    mutable std::mutex  m_mutex;

    std::unordered_map<std::string, ModuleInstance> m_modules;
    uint64_t                                        m_nextLoadOrder = 0;

    ModuleEventCallback m_eventCallback;
};

} // namespace SagaEngine::Core::Modules
