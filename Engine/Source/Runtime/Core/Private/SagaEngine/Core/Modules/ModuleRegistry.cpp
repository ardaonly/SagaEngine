/// @file ModuleRegistry.cpp
/// @brief Static module registry implementation.

#include "SagaEngine/Core/Modules/ModuleRegistry.h"
#include "SagaEngine/Core/Log/Log.h"

#include <mutex>
#include <utility>

namespace SagaEngine::Core::Modules {

namespace
{
constexpr const char* kTag = "ModuleRegistry";
}

ModuleRegistry& ModuleRegistry::Instance() noexcept
{
    static ModuleRegistry instance;
    return instance;
}

void ModuleRegistry::RegisterStatic(const char* name, CreateModuleFn factory,
                                    ModuleDescriptor descriptor) noexcept
{
    if (!name || !factory)
    {
        return;
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_factories.count(name) != 0)
    {
        LOG_WARN(kTag, "Module '%s' already registered; skipping duplicate", name);
        return;
    }

    ModuleFactoryEntry entry;
    entry.factory = factory;
    entry.descriptor = std::move(descriptor);
    m_factories.emplace(name, std::move(entry));

    const auto& stored = m_factories.at(name).descriptor;
    LOG_DEBUG(kTag, "Registered static module: '%s' v%s",
              stored.name ? stored.name : name,
              stored.version ? stored.version : "");
}

const ModuleFactoryEntry* ModuleRegistry::GetFactory(const char* name) const noexcept
{
    if (!name)
    {
        return nullptr;
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    const auto it = m_factories.find(name);
    return it != m_factories.end() ? &it->second : nullptr;
}

std::vector<std::string> ModuleRegistry::GetRegisteredNames() const noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<std::string> names;
    names.reserve(m_factories.size());
    for (const auto& [name, _] : m_factories)
    {
        names.push_back(name);
    }
    return names;
}

bool ModuleRegistry::IsRegistered(const char* name) const noexcept
{
    if (!name)
    {
        return false;
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    return m_factories.count(name) != 0;
}

ModuleDescriptor ModuleRegistry::GetDescriptor(const char* name) const noexcept
{
    if (!name)
    {
        return ModuleDescriptor{};
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    const auto it = m_factories.find(name);
    return it != m_factories.end() ? it->second.descriptor : ModuleDescriptor{};
}

uint32_t ModuleRegistry::Count() const noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return static_cast<uint32_t>(m_factories.size());
}

} // namespace SagaEngine::Core::Modules
