/// @file ModuleRegistry.cpp
/// @brief Module registry implementation with static and dynamic support.

#include "SagaEngine/Core/Modules/ModuleRegistry.h"
#include "SagaEngine/Core/Log/Log.h"

#include <mutex>

#if defined(_WIN32) || defined(_WIN64)
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
#else
    #include <dlfcn.h>
    #include <dirent.h>
#endif

namespace SagaEngine::Core::Modules {

static constexpr const char* kTag = "ModuleRegistry";

// ─── Singleton ──────────────────────────────────────────────────────────

ModuleRegistry& ModuleRegistry::Instance() noexcept
{
    static ModuleRegistry instance;
    return instance;
}

// ─── Static registration ────────────────────────────────────────────────

void ModuleRegistry::RegisterStatic(const char* name, CreateModuleFn factory,
                                     ModuleDescriptor descriptor) noexcept
{
    if (!name || !factory)
        return;

    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_factories.count(name) != 0)
    {
        LOG_WARN(kTag, "Module '%s' already registered; skipping duplicate", name);
        return;
    }

    ModuleFactoryEntry entry;
    entry.factory = factory;
    entry.libraryHandle = nullptr;  // Static linkage
    entry.libraryPath.clear();
    entry.descriptor = std::move(descriptor);

    m_factories.emplace(name, std::move(entry));

    LOG_DEBUG(kTag, "Registered static module: '%s' v%s",
              entry.descriptor.name, entry.descriptor.version);
}

// ─── Dynamic discovery (platform-specific) ──────────────────────────────

uint32_t ModuleRegistry::ScanPluginsDirectory(const char* path) noexcept
{
    if (!path)
        return 0;

    uint32_t count = 0;

#if defined(_WIN32) || defined(_WIN64)
    // Windows: scan for .dll files.
    WIN32_FIND_DATAA findData;
    char searchPath[MAX_PATH];
    snprintf(searchPath, sizeof(searchPath), "%s\\*.dll", path);

    HANDLE hFind = FindFirstFileA(searchPath, &findData);
    if (hFind == INVALID_HANDLE_VALUE)
    {
        LOG_WARN(kTag, "Plugins directory not found: %s", path);
        return 0;
    }

    do
    {
        if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            continue;  // Skip directories.

        char libraryPath[MAX_PATH];
        snprintf(libraryPath, sizeof(libraryPath), "%s\\%s", path, findData.cFileName);

        if (LoadPluginLibrary(libraryPath))
            count++;
    }
    while (FindNextFileA(hFind, &findData) != 0);

    FindClose(hFind);

#else
    // POSIX: scan for .so (Linux) or .dylib (macOS) files.
    DIR* dir = opendir(path);
    if (!dir)
    {
        LOG_WARN(kTag, "Plugins directory not found: %s", path);
        return 0;
    }

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr)
    {
        const char* ext = strrchr(entry->d_name, '.');
        if (!ext)
            continue;

#if defined(__APPLE__)
        const bool isPlugin = (strcmp(ext, ".dylib") == 0);
#else
        const bool isPlugin = (strcmp(ext, ".so") == 0);
#endif

        if (!isPlugin)
            continue;

        char libraryPath[512];
        snprintf(libraryPath, sizeof(libraryPath), "%s/%s", path, entry->d_name);

        if (LoadPluginLibrary(libraryPath))
            count++;
    }

    closedir(dir);
#endif

    LOG_INFO(kTag, "Discovered %u plugin(s) in '%s'", count, path);
    return count;
}

bool ModuleRegistry::LoadPluginLibrary(const char* libraryPath) noexcept
{
#if defined(_WIN32) || defined(_WIN64)
    HMODULE hLib = LoadLibraryA(libraryPath);
    if (!hLib)
    {
        LOG_WARN(kTag, "Failed to load plugin: %s (error %lu)",
                 libraryPath, GetLastError());
        return false;
    }

    CreateModuleFn factory = reinterpret_cast<CreateModuleFn>(
        GetProcAddress(hLib, "CreateModule"));

#else
    void* hLib = dlopen(libraryPath, RTLD_NOW | RTLD_LOCAL);
    if (!hLib)
    {
        LOG_WARN(kTag, "Failed to load plugin: %s (%s)",
                 libraryPath, dlerror());
        return false;
    }

    CreateModuleFn factory = reinterpret_cast<CreateModuleFn>(
        dlsym(hLib, "CreateModule"));

#endif

    if (!factory)
    {
        LOG_WARN(kTag, "Plugin '%s' does not export CreateModule()", libraryPath);

#if defined(_WIN32) || defined(_WIN64)
        FreeLibrary(hLib);
#else
        dlclose(hLib);
#endif

        return false;
    }

    // Create a temporary instance to query the descriptor.
    IModule* tempModule = factory();
    if (!tempModule)
    {
        LOG_ERROR(kTag, "Plugin '%s' CreateModule() returned nullptr", libraryPath);

#if defined(_WIN32) || defined(_WIN64)
        FreeLibrary(hLib);
#else
        dlclose(hLib);
#endif

        return false;
    }

    ModuleDescriptor desc = tempModule->GetDescriptor();
    delete tempModule;  // Don't keep it; just query the descriptor.

    // Register the factory.
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_factories.count(desc.name) != 0)
    {
        LOG_WARN(kTag, "Module '%s' from '%s' already registered; skipping",
                 desc.name, libraryPath);

#if defined(_WIN32) || defined(_WIN64)
        FreeLibrary(hLib);
#else
        dlclose(hLib);
#endif

        return false;
    }

    ModuleFactoryEntry entry;
    entry.factory = factory;
    entry.libraryHandle = hLib;
    entry.libraryPath = libraryPath;
    entry.descriptor = std::move(desc);

    m_factories.emplace(entry.descriptor.name, std::move(entry));

    LOG_INFO(kTag, "Loaded dynamic module: '%s' v%s from '%s'",
             entry.descriptor.name, entry.descriptor.version, libraryPath);

    return true;
}

// ─── Queries ────────────────────────────────────────────────────────────

const ModuleFactoryEntry* ModuleRegistry::GetFactory(const char* name) const noexcept
{
    if (!name)
        return nullptr;

    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_factories.find(name);
    return (it != m_factories.end()) ? &it->second : nullptr;
}

std::vector<std::string> ModuleRegistry::GetRegisteredNames() const noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<std::string> names;
    names.reserve(m_factories.size());

    for (const auto& [name, _] : m_factories)
        names.push_back(name);

    return names;
}

bool ModuleRegistry::IsRegistered(const char* name) const noexcept
{
    if (!name)
        return false;

    std::lock_guard<std::mutex> lock(m_mutex);
    return m_factories.count(name) != 0;
}

ModuleDescriptor ModuleRegistry::GetDescriptor(const char* name) const noexcept
{
    if (!name)
        return ModuleDescriptor{};

    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_factories.find(name);
    return (it != m_factories.end()) ? it->second.descriptor : ModuleDescriptor{};
}

uint32_t ModuleRegistry::Count() const noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return static_cast<uint32_t>(m_factories.size());
}

std::vector<std::string> ModuleRegistry::GetPluginPaths() const noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<std::string> paths;

    for (const auto& [_, entry] : m_factories)
    {
        if (!entry.libraryPath.empty())
            paths.push_back(entry.libraryPath);
    }

    return paths;
}

} // namespace SagaEngine::Core::Modules
