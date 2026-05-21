/// @file SagaLowLevelScriptingApiTests.cpp
/// @brief Compile test for low-level-only public SagaScript includes.

#include <SagaEngine/Scripting/LowLevel/ISagaScriptHost.hpp>
#include <SagaEngine/Scripting/LowLevel/ScriptLifecycleContracts.hpp>
#include <SagaEngine/Scripting/LowLevel/ScriptLifecycleService.hpp>
#include <SagaEngine/Scripting/LowLevel/ScriptPackageValidator.hpp>

#include <gtest/gtest.h>

#include <string>
#include <string_view>
#include <type_traits>

namespace
{

static_assert(std::is_abstract_v<Saga::Scripting::ISagaScriptHost>);
static_assert(std::is_abstract_v<Saga::Scripting::ISagaScriptWorld>);

} // namespace

TEST(SagaLowLevelScriptingApiTests, LowLevelHeadersExposeLifecycleHostContracts)
{
    Saga::Scripting::ScriptPackageHandle package{11};
    Saga::Scripting::ScriptInstanceHandle instance{22};
    Saga::Scripting::ScriptLifecycleInvocation invocation;
    invocation.instance = instance;
    invocation.method = Saga::Scripting::ScriptLifecycleMethod::OnUpdate;
    invocation.deltaTimeSeconds = 0.25;

    Saga::Scripting::ScriptPackageValidationOptions validationOptions;
    Saga::Scripting::ScriptPackageLoadRequest loadRequest;
    Saga::Scripting::ISagaScriptHost* host = nullptr;
    Saga::Scripting::ScriptLifecycleService* lifecycle = nullptr;

    EXPECT_TRUE(package.IsValid());
    EXPECT_TRUE(invocation.instance.IsValid());
    EXPECT_EQ(
        Saga::Scripting::ToString(invocation.method),
        std::string_view("OnUpdate"));
    EXPECT_EQ(validationOptions.expectedTargetFramework, "net10.0");
    EXPECT_TRUE(loadRequest.packageId.empty());
    EXPECT_EQ(host, nullptr);
    EXPECT_EQ(lifecycle, nullptr);
}
