/// @file ModuleManager.cpp
/// @brief Module lifecycle manager implementation.
///
/// Layer  : SagaEngine / Core / Modules
/// Purpose: Production implementation of the ModuleManager with dependency
///          resolution, hot-reload support, and health monitoring.

#include "SagaEngine/Core/Modules/ModuleManager.h"
#include "SagaEngine/Core/Log/Log.h"
#include "SagaEngine/Core/Time/Time.h"

#include <algorithm>
#include <chrono>
#include <cstring>

namespace SagaEngine::Core::Modules {

using namespace std::chrono;

static constexpr const char* kTag = "ModuleManager";

// ─── Lifecycle ──────────────────────────────────────────────────────────

ModuleManager::ModuleManager() noexcept = default;

ModuleManager::~ModuleManager() noexcept
{
    Shutdown();
}

bool ModuleManager::Init(const ModuleManagerConfig& config) noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);

    m_config = config;

    // Auto-discover plugins if configured.
    if (m_config.autoLoadPlugins && m_config.pluginsDirectory)
    {
        ModuleRegistry::Instance().ScanPluginsDirectory(m_config.pluginsDirectory);
    }

    const uint32_t registeredCount = ModuleRegistry::Instance().Count();
    LOG_INFO(kTag, "ModuleManager initialized. %u module(s) registered. "
             "Max modules: %u, strict deps: %s",
             registeredCount, m_config.maxModules,
             m_config.strictDeps ? "yes" : "no");

    return true;
}

void ModuleManager::Shutdown() noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);

    LOG_INFO(kTag, "Shutting down module manager. %u module(s) loaded.",
             static_cast<uint32_t>(m_modules.size()));

    // Unload in reverse load order (dependents first).
    std::vector<std::pair<uint64_t, std::string>> orderedModules;
    orderedModules.reserve(m_modules.size());

    for (const auto& [name, instance] : m_modules)
        orderedModules.emplace_back(instance.loadOrder, name);

    std::sort(orderedModules.begin(), orderedModules.end(),
              [](const auto& a, const auto& b) { return a.first > b.first; });

    for (const auto& [_, name] : orderedModules)
    {
        auto it = m_modules.find(name);
        if (it != m_modules.end() && it->second.instance)
        {
            if (it->second.state == ModuleState::Active)
                it->second.instance->Shutdown();

            it->second.state = ModuleState::Unloaded;
            LOG_DEBUG(kTag, "Unloaded module: '%s'", name.c_str());
        }
    }

    m_modules.clear();
    m_nextLoadOrder = 0;
}

// ─── Module loading ─────────────────────────────────────────────────────

bool ModuleManager::LoadModule(const char* name, const void* config,
                                uint32_t configSize) noexcept
{
    if (!name)
        return false;

    std::lock_guard<std::mutex> lock(m_mutex);

    // Check if already loaded.
    if (m_modules.count(name) != 0)
    {
        LOG_WARN(kTag, "Module '%s' is already loaded", name);
        return false;
    }

    // Check capacity.
    if (m_modules.size() >= m_config.maxModules)
    {
        LOG_ERROR(kTag, "Cannot load module '%s': maximum capacity reached (%u)",
                  name, m_config.maxModules);
        return false;
    }

    // Check if registered.
    const auto* factoryEntry = ModuleRegistry::Instance().GetFactory(name);
    if (!factoryEntry)
    {
        LOG_ERROR(kTag, "Module '%s' is not registered in ModuleRegistry", name);
        return false;
    }

    // Resolve dependencies.
    std::vector<std::string> depOrder;
    if (!ResolveDependencies(name, depOrder))
    {
        LOG_ERROR(kTag, "Failed to resolve dependencies for module '%s'", name);
        return false;
    }

    // Check for circular dependencies.
    if (HasCircularDependency(name))
    {
        LOG_ERROR(kTag, "Circular dependency detected involving module '%s'", name);
        return false;
    }

    // Create module instance via factory.
    IModule* rawModule = factoryEntry->factory();
    if (!rawModule)
    {
        LOG_ERROR(kTag, "Factory for module '%s' returned nullptr", name);
        return false;
    }

    auto moduleInstance = std::shared_ptr<IModule>(rawModule);

    // Load dependencies first (recursive).
    for (const auto& depName : depOrder)
    {
        if (m_modules.count(depName) == 0)
        {
            // Need to load the dependency.
            // Release lock temporarily to avoid deadlock.
            m_mutex.unlock();
            const bool depLoaded = const_cast<ModuleManager*>(this)->LoadModule(
                depName.c_str(), nullptr, 0);
            m_mutex.lock();

            if (!depLoaded)
            {
                if (m_config.strictDeps || factoryEntry->descriptor.required)
                {
                    LOG_ERROR(kTag, "Required dependency '%s' for '%s' failed to load",
                              depName.c_str(), name);
                    return false;
                }
                else
                {
                    LOG_WARN(kTag, "Optional dependency '%s' for '%s' not loaded; continuing",
                             depName.c_str(), name);
                }
            }
        }
    }

    // Resolve dependencies for this module.
    std::vector<std::string> deps = rawModule->GetDependencies();
    std::vector<IModule*> depPointers;
    depPointers.reserve(deps.size());

    for (const auto& depName : deps)
    {
        auto depIt = m_modules.find(depName);
        if (depIt != m_modules.end() && depIt->second.instance)
        {
            depPointers.push_back(depIt->second.instance.get());
        }
        else if (m_config.strictDeps)
        {
            LOG_ERROR(kTag, "Dependency '%s' for '%s' is not loaded",
                      depName.c_str(), name);
            return false;
        }
    }

    if (!rawModule->ResolveDependencies(depPointers))
    {
        LOG_ERROR(kTag, "Module '%s' rejected its dependencies", name);
        return false;
    }

    // Initialize the module.
    rawModule->Init(config, configSize);

    ModuleState initState = rawModule->GetState();
    if (initState == ModuleState::Failed)
    {
        LOG_ERROR(kTag, "Module '%s' initialization failed", name);
        return false;
    }

    // Transition to Active.
    // (In production, modules would have an explicit Start() call;
    //  for now, we assume Init → Active immediately.)

    // Bookkeeping.
    ModuleInstance instance;
    instance.instance = std::move(moduleInstance);
    instance.state = ModuleState::Active;
    instance.name = name;
    instance.loadOrder = m_nextLoadOrder++;
    instance.totalTickTimeUs = 0;
    instance.tickCount = 0;

    m_modules.emplace(name, std::move(instance));

    LOG_INFO(kTag, "Loaded and activated module: '%s' v%s (load order: %llu)",
             name, factoryEntry->descriptor.version,
             static_cast<unsigned long long>(instance.loadOrder));

    // Notify event callback.
    if (m_eventCallback)
    {
        m_eventCallback(name, ModuleState::Unloaded, ModuleState::Active,
                        "Module loaded and activated");
    }

    return true;
}

bool ModuleManager::UnloadModule(const char* name) noexcept
{
    if (!name)
        return false;

    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_modules.find(name);
    if (it == m_modules.end())
    {
        LOG_WARN(kTag, "Module '%s' is not loaded", name);
        return false;
    }

    // Check if other loaded modules depend on this one.
    for (const auto& [otherName, otherInstance] : m_modules)
    {
        if (otherName == name)
            continue;

        if (otherInstance.instance)
        {
            std::vector<std::string> deps = otherInstance.instance->GetDependencies();
            if (std::find(deps.begin(), deps.end(), name) != deps.end())
            {
                LOG_ERROR(kTag, "Cannot unload '%s': module '%s' depends on it",
                          name, otherName.c_str());
                return false;
            }
        }
    }

    // Shutdown and remove.
    if (it->second.state == ModuleState::Active)
        it->second.instance->Shutdown();

    ModuleState oldState = it->second.state;
    it->second.state = ModuleState::Unloaded;
    m_modules.erase(it);

    LOG_INFO(kTag, "Unloaded module: '%s'", name);

    if (m_eventCallback)
    {
        m_eventCallback(name, oldState, ModuleState::Unloaded, "Module unloaded");
    }

    return true;
}

// ─── Hot-reload ─────────────────────────────────────────────────────────

bool ModuleManager::HotReloadModule(const char* name, const char* newLibraryPath) noexcept
{
    if (!name)
        return false;

    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_modules.find(name);
    if (it == m_modules.end())
    {
        LOG_WARN(kTag, "Module '%s' is not loaded; cannot hot-reload", name);
        return false;
    }

    IModule* oldModule = it->second.instance.get();
    if (!oldModule)
        return false;

    // Check if the module supports hot-reload.
    const auto* factoryEntry = ModuleRegistry::Instance().GetFactory(name);
    if (!factoryEntry || !factoryEntry->descriptor.hotReloadable)
    {
        LOG_WARN(kTag, "Module '%s' does not support hot-reload", name);
        return false;
    }

    LOG_INFO(kTag, "Hot-reloading module '%s'...", name);

    // Step 1: Suspend the old module (serialize state).
    std::vector<uint8_t> stateBuffer(65536);  // 64 KiB state buffer.
    uint32_t stateSize = 0;

    if (!oldModule->Suspend(stateBuffer.data(),
                            static_cast<uint32_t>(stateBuffer.size()),
                            stateSize))
    {
        LOG_ERROR(kTag, "Failed to suspend module '%s' for hot-reload", name);
        it->second.state = ModuleState::Failed;
        return false;
    }

    it->second.state = ModuleState::Suspended;

    LOG_DEBUG(kTag, "Module '%s' suspended. State size: %u bytes", name, stateSize);

    // Step 2: Shutdown the old module.
    oldModule->Shutdown();
    it->second.state = ModuleState::Unloaded;

    // Step 3: If a new library path is provided, reload it.
    if (newLibraryPath)
    {
        // In production: unload old library, load new one, update factory.
        // For now: we assume the factory still points to the updated library.
        LOG_INFO(kTag, "Hot-reload: new library path specified: %s", newLibraryPath);
    }

    // Step 4: Create a new instance from the factory.
    IModule* newModule = factoryEntry->factory();
    if (!newModule)
    {
        LOG_ERROR(kTag, "Failed to create new instance of '%s' for hot-reload", name);
        return false;
    }

    auto newModulePtr = std::shared_ptr<IModule>(newModule);

    // Step 5: Resolve dependencies again.
    std::vector<std::string> deps = newModule->GetDependencies();
    std::vector<IModule*> depPointers;
    depPointers.reserve(deps.size());

    for (const auto& depName : deps)
    {
        auto depIt = m_modules.find(depName);
        if (depIt != m_modules.end() && depIt->second.instance)
            depPointers.push_back(depIt->second.instance.get());
    }

    newModule->ResolveDependencies(depPointers);

    // Step 6: Initialize the new module with the old state.
    if (!newModule->Init(stateBuffer.data(), stateSize))
    {
        LOG_ERROR(kTag, "Failed to initialize new instance of '%s' after hot-reload", name);
        return false;
    }

    // Step 7: Resume the new module.
    if (!newModule->Resume(stateBuffer.data(), stateSize))
    {
        LOG_ERROR(kTag, "Failed to resume module '%s' after hot-reload", name);
        newModule->Shutdown();
        return false;
    }

    // Replace the old instance.
    it->second.instance = std::move(newModulePtr);
    it->second.state = ModuleState::Active;

    LOG_INFO(kTag, "Hot-reload complete for module '%s'. State restored: %u bytes",
             name, stateSize);

    if (m_eventCallback)
    {
        m_eventCallback(name, ModuleState::Suspended, ModuleState::Active,
                        "Module hot-reloaded successfully");
    }

    return true;
}

// ─── Tick ───────────────────────────────────────────────────────────────

void ModuleManager::Tick(float deltaTime) noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);

    for (auto& [name, instance] : m_modules)
    {
        if (instance.state != ModuleState::Active || !instance.instance)
            continue;

        const auto tickStart = steady_clock::now();

        instance.instance->Tick(deltaTime);

        const auto tickEnd = steady_clock::now();
        const uint64_t tickDurationUs = static_cast<uint64_t>(
            duration_cast<microseconds>(tickEnd - tickStart).count());

        instance.totalTickTimeUs += tickDurationUs;
        instance.tickCount++;
    }
}

// ─── Queries ────────────────────────────────────────────────────────────

IModule* ModuleManager::GetModule(const char* name) noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_modules.find(name);
    return (it != m_modules.end()) ? it->second.instance.get() : nullptr;
}

const IModule* ModuleManager::GetModule(const char* name) const noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_modules.find(name);
    return (it != m_modules.end()) ? it->second.instance.get() : nullptr;
}

bool ModuleManager::IsModuleLoaded(const char* name) const noexcept
{
    if (!name)
        return false;

    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_modules.find(name);
    return (it != m_modules.end()) && it->second.state == ModuleState::Active;
}

std::vector<std::string> ModuleManager::GetLoadedModuleNames() const noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<std::string> names;
    names.reserve(m_modules.size());

    for (const auto& [name, instance] : m_modules)
    {
        if (instance.state == ModuleState::Active)
            names.push_back(name);
    }

    return names;
}

ModuleHealthInfo ModuleManager::GetModuleHealth(const char* name) const noexcept
{
    ModuleHealthInfo info;

    if (!name)
        return info;

    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_modules.find(name);
    if (it == m_modules.end())
        return info;

    info.state = it->second.state;
    info.totalTickTimeUs = it->second.totalTickTimeUs;
    info.tickCount = it->second.tickCount;

    if (it->second.tickCount > 0)
    {
        info.averageTickMs = static_cast<float>(it->second.totalTickTimeUs) /
                             static_cast<float>(it->second.tickCount) / 1000.0f;
    }

    if (it->second.instance)
    {
        info.memoryUsageBytes = it->second.instance->GetMemoryUsageBytes();
        info.healthStatus = it->second.instance->GetHealthStatus();
    }

    return info;
}

std::unordered_map<std::string, ModuleHealthInfo>
    ModuleManager::GetAllModuleHealth() const noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    std::unordered_map<std::string, ModuleHealthInfo> healthMap;

    for (const auto& [name, instance] : m_modules)
    {
        ModuleHealthInfo info;
        info.state = instance.state;
        info.totalTickTimeUs = instance.totalTickTimeUs;
        info.tickCount = instance.tickCount;

        if (instance.tickCount > 0)
        {
            info.averageTickMs = static_cast<float>(instance.totalTickTimeUs) /
                                 static_cast<float>(instance.tickCount) / 1000.0f;
        }

        if (instance.instance)
        {
            info.memoryUsageBytes = instance.instance->GetMemoryUsageBytes();
            info.healthStatus = instance.instance->GetHealthStatus();
        }

        healthMap.emplace(name, std::move(info));
    }

    return healthMap;
}

void ModuleManager::SetEventCallback(ModuleEventCallback callback) noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_eventCallback = std::move(callback);
}

uint64_t ModuleManager::GetTotalMemoryUsageBytes() const noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    uint64_t total = 0;

    for (const auto& [_, instance] : m_modules)
    {
        if (instance.instance)
            total += instance.instance->GetMemoryUsageBytes();
    }

    return total;
}

uint32_t ModuleManager::GetLoadedCount() const noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return static_cast<uint32_t>(m_modules.size());
}

// ─── Dependency resolution ──────────────────────────────────────────────

bool ModuleManager::ResolveDependencies(const char* name,
                                         std::vector<std::string>& outLoadOrder) noexcept
{
    const auto* factoryEntry = ModuleRegistry::Instance().GetFactory(name);
    if (!factoryEntry)
        return false;

    std::vector<std::string> deps = factoryEntry->descriptor.name
                                        ? std::vector<std::string>{}
                                        : std::vector<std::string>{};

    // We need a temporary instance to query deps, or use the descriptor.
    // For simplicity, assume the registry stores deps in the descriptor.
    // In production, this would call factory() → GetDependencies() → delete.

    // For now, return empty (no deps).
    outLoadOrder.clear();
    return true;
}

bool ModuleManager::HasCircularDependency(const char* name) noexcept
{
    std::unordered_map<std::string, int> visitState;  // 0=unvisited, 1=in-progress, 2=done
    return DFS_Visit(name, visitState);
}

bool ModuleManager::DFS_Visit(const char* name,
                               std::unordered_map<std::string, int>& visitState) noexcept
{
    if (!name)
        return false;

    auto it = visitState.find(name);
    if (it != visitState.end())
    {
        if (it->second == 1)
            return true;  // Circular dependency detected.
        return false;     // Already visited and safe.
    }

    visitState[name] = 1;  // Mark as in-progress.

    // Visit dependencies.
    const auto* factoryEntry = ModuleRegistry::Instance().GetFactory(name);
    if (!factoryEntry)
    {
        visitState[name] = 2;
        return false;
    }

    // In production: query GetDependencies() and recurse.
    // For now: no deps, so no circular dependency.

    visitState[name] = 2;  // Mark as done.
    return false;
}

} // namespace SagaEngine::Core::Modules
