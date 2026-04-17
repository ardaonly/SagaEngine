/// @file ModuleRegistry.h
/// @brief Registry for module factories and descriptors (static + dynamic).
///
/// Layer  : SagaEngine / Core / Modules
/// Purpose: The ModuleRegistry maintains a catalog of known modules — both
///          statically linked and dynamically loaded shared libraries.
///          It provides the ModuleManager with a lookup table to instantiate
///          modules by name, query their descriptors, and discover available
///          modules at runtime.
///
/// Design rules:
///   - Registry is a singleton accessible globally (read-only after boot)
///   - Modules can be registered statically at compile time
///   - Dynamic modules are discovered by scanning a plugins directory
///   - Each module has a unique name (case-sensitive string)
///   - The registry is immutable after engine boot (thread-safe reads)

#pragma once

#include "SagaEngine/Core/Modules/IModule.h"

#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace SagaEngine::Core::Modules {

// ─── Module factory wrapper ─────────────────────────────────────────────

/// Wraps a module factory function with metadata about its source.
struct ModuleFactoryEntry
{
    CreateModuleFn  factory         = nullptr;   ///< Factory function (static or from dlsym).
    void*           libraryHandle   = nullptr;   ///< Opaque library handle (nullptr for static).
    std::string     libraryPath;                  ///< Path to .dll/.so (empty for static).
    ModuleDescriptor descriptor;                   ///< Static descriptor from the factory.
};

// ─── Module registry ────────────────────────────────────────────────────

/// Singleton registry for all known module factories.
///
/// Usage:
///   1. At engine boot: ModuleRegistry::Instance().RegisterStatic<MyModule>()
///   2. Optionally: ModuleRegistry::Instance().ScanPluginsDirectory(path)
///   3. ModuleManager queries the registry to load modules by name
///
/// Thread model:
///   Registration is mutex-protected; lookups are lock-free after boot.
class ModuleRegistry
{
public:
    /// Get the singleton instance.
    static ModuleRegistry& Instance() noexcept;

    ModuleRegistry(const ModuleRegistry&)            = delete;
    ModuleRegistry& operator=(const ModuleRegistry&) = delete;

    // ─── Static registration ──────────────────────────────────────────

    /// Register a statically linked module factory.
    /// Call this at engine boot before any modules are loaded.
    /// @param name       Unique module name (must match IModule::GetDescriptor().name).
    /// @param factory    Factory function (typically &CreateModule from the module).
    /// @param descriptor Static descriptor for the module.
    void RegisterStatic(const char* name, CreateModuleFn factory,
                        ModuleDescriptor descriptor) noexcept;

    // ─── Dynamic discovery ────────────────────────────────────────────

    /// Scan a directory for plugin shared libraries (.dll on Windows,
    /// .so on Linux, .dylib on macOS).  Each library must export
    /// CreateModule() and a ModuleDescriptor symbol.
    /// @param path  Directory path to scan.
    /// @return Number of successfully discovered modules.
    uint32_t ScanPluginsDirectory(const char* path) noexcept;

    // ─── Queries ──────────────────────────────────────────────────────

    /// Get a factory entry by module name.
    /// @param name  Module name (case-sensitive).
    /// @return Pointer to factory entry, or nullptr if not found.
    [[nodiscard]] const ModuleFactoryEntry* GetFactory(const char* name) const noexcept;

    /// Get all registered module names.
    [[nodiscard]] std::vector<std::string> GetRegisteredNames() const noexcept;

    /// Check if a module is registered.
    [[nodiscard]] bool IsRegistered(const char* name) const noexcept;

    /// Get descriptor for a registered module.
    [[nodiscard]] ModuleDescriptor GetDescriptor(const char* name) const noexcept;

    // ─── Diagnostics ──────────────────────────────────────────────────

    /// Total number of registered modules (static + dynamic).
    [[nodiscard]] uint32_t Count() const noexcept;

    /// List of all plugin library paths (empty for static modules).
    [[nodiscard]] std::vector<std::string> GetPluginPaths() const noexcept;

private:
    ModuleRegistry() noexcept = default;
    ~ModuleRegistry() noexcept = default;

    // Internal helper for dynamic library loading.
    bool LoadPluginLibrary(const char* libraryPath) noexcept;

    mutable std::mutex m_mutex;
    std::unordered_map<std::string, ModuleFactoryEntry> m_factories;
};

// ─── Static registration helper ─────────────────────────────────────────

/// Helper to register a module at global scope (before main).
/// Usage: SAGA_REGISTER_MODULE_STATIC(MyModule) in a .cpp file.
/// This creates a static initializer that registers the module factory.
#define SAGA_REGISTER_MODULE_STATIC(ModuleClass) \
    namespace { \
        static struct ModuleClass##_Registrar { \
            ModuleClass##_Registrar() \
            { \
                ::SagaEngine::Core::Modules::ModuleDescriptor desc = {}; \
                desc.name = #ModuleClass; \
                desc.version = "0.0.1"; \
                desc.hotReloadable = false; \
                desc.required = true; \
                ::SagaEngine::Core::Modules::ModuleRegistry::Instance().RegisterStatic( \
                    #ModuleClass, \
                    []() -> ::SagaEngine::Core::Modules::IModule* { return new ModuleClass(); }, \
                    std::move(desc)); \
            } \
        } s_##ModuleClass##_registrar_instance; \
    } // namespace

} // namespace SagaEngine::Core::Modules
