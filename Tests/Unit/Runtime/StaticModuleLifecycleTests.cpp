// SPDX-License-Identifier: Apache-2.0

#include <SagaEngine/Core/Modules/ModuleManager.h>
#include <SagaEngine/Core/Modules/ModuleRegistry.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

namespace
{
using namespace SagaEngine::Core::Modules;

struct LifecycleCounters
{
    uint32_t initialized = 0;
    uint32_t ticks = 0;
    uint32_t shutdowns = 0;
};

LifecycleCounters g_counters;

class StaticLifecycleModule final : public IModule
{
public:
    [[nodiscard]] ModuleDescriptor GetDescriptor() const noexcept override
    {
        ModuleDescriptor descriptor;
        descriptor.name = "StaticLifecycleModule";
        descriptor.version = "0.0.11";
        descriptor.author = "SagaEngine";
        descriptor.apiVersion = 1;
        descriptor.required = true;
        return descriptor;
    }

    [[nodiscard]] std::vector<std::string> GetDependencies() const noexcept override
    {
        return {};
    }

    bool ResolveDependencies(const std::vector<IModule*>& dependencies) noexcept override
    {
        return dependencies.empty();
    }

    bool Init(const void*, uint32_t) noexcept override
    {
        ++g_counters.initialized;
        state_ = ModuleState::Active;
        return true;
    }

    void Tick(float) noexcept override
    {
        ++g_counters.ticks;
    }

    void Shutdown() noexcept override
    {
        ++g_counters.shutdowns;
        state_ = ModuleState::Unloaded;
    }

    [[nodiscard]] ModuleState GetState() const noexcept override
    {
        return state_;
    }

    [[nodiscard]] std::string GetHealthStatus() const noexcept override
    {
        return "healthy";
    }

    [[nodiscard]] uint64_t GetMemoryUsageBytes() const noexcept override
    {
        return 128;
    }

private:
    ModuleState state_ = ModuleState::Unloaded;
};

IModule* CreateStaticLifecycleModule()
{
    return new StaticLifecycleModule();
}

void RegisterLifecycleModule()
{
    ModuleDescriptor descriptor;
    descriptor.name = "StaticLifecycleModule";
    descriptor.version = "0.0.11";
    descriptor.author = "SagaEngine";
    descriptor.apiVersion = 1;
    descriptor.required = true;
    ModuleRegistry::Instance().RegisterStatic(
        descriptor.name, &CreateStaticLifecycleModule, descriptor);
}
} // namespace

TEST(StaticModuleLifecycleTests, RegistryKeepsStaticFactoryAndDescriptor)
{
    RegisterLifecycleModule();

    auto& registry = ModuleRegistry::Instance();
    ASSERT_TRUE(registry.IsRegistered("StaticLifecycleModule"));
    ASSERT_NE(registry.GetFactory("StaticLifecycleModule"), nullptr);

    const auto descriptor = registry.GetDescriptor("StaticLifecycleModule");
    ASSERT_NE(descriptor.name, nullptr);
    EXPECT_STREQ(descriptor.name, "StaticLifecycleModule");
    EXPECT_STREQ(descriptor.version, "0.0.11");

    const auto names = registry.GetRegisteredNames();
    EXPECT_NE(std::find(names.begin(), names.end(), "StaticLifecycleModule"),
              names.end());
}

TEST(StaticModuleLifecycleTests, ManagerRunsBoundedStaticLifecycle)
{
    RegisterLifecycleModule();
    g_counters = {};

    ModuleManager manager;
    ModuleManagerConfig config;
    config.maxModules = 4;
    config.strictDeps = true;

    ASSERT_TRUE(manager.Init(config));
    ASSERT_TRUE(manager.LoadModule("StaticLifecycleModule"));
    EXPECT_TRUE(manager.IsModuleLoaded("StaticLifecycleModule"));
    EXPECT_EQ(manager.GetLoadedCount(), 1U);
    EXPECT_EQ(manager.GetTotalMemoryUsageBytes(), 128U);

    manager.Tick(1.0F / 60.0F);
    const auto health = manager.GetModuleHealth("StaticLifecycleModule");
    EXPECT_EQ(health.state, ModuleState::Active);
    EXPECT_EQ(health.tickCount, 1U);
    EXPECT_EQ(health.healthStatus, "healthy");

    ASSERT_TRUE(manager.UnloadModule("StaticLifecycleModule"));
    EXPECT_FALSE(manager.IsModuleLoaded("StaticLifecycleModule"));
    EXPECT_EQ(g_counters.initialized, 1U);
    EXPECT_EQ(g_counters.ticks, 1U);
    EXPECT_EQ(g_counters.shutdowns, 1U);
}
