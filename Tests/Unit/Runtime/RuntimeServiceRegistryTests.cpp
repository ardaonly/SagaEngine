/// @file RuntimeServiceRegistryTests.cpp
/// @brief Focused tests for Runtime service lifecycle registry behavior.

#include <SagaRuntime/RuntimeServiceRegistry.hpp>

#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace
{

using SagaRuntime::IRuntimeService;
using SagaRuntime::RuntimeServiceDescriptor;
using SagaRuntime::RuntimeServiceDiagnostic;
using SagaRuntime::RuntimeServiceDiagnosticSeverity;
using SagaRuntime::RuntimeServiceLifecycle;
using SagaRuntime::RuntimeServiceLifecycleResult;
using SagaRuntime::RuntimeServiceRegistry;
using SagaRuntime::RuntimeServiceState;

struct ServiceCallLog
{
    std::vector<std::string> entries;
};

[[nodiscard]] RuntimeServiceDescriptor DescriptorFor(const std::string& serviceId)
{
    RuntimeServiceDescriptor descriptor;
    descriptor.serviceId = serviceId;
    descriptor.displayName = serviceId + " Service";
    descriptor.category = "test";
    return descriptor;
}

[[nodiscard]] RuntimeServiceLifecycleResult ResultFor(
    RuntimeServiceDescriptor descriptor,
    RuntimeServiceState state,
    std::vector<RuntimeServiceDiagnostic> diagnostics = {})
{
    RuntimeServiceLifecycleResult result;
    result.descriptor = std::move(descriptor);
    result.state = state;
    result.diagnostics = std::move(diagnostics);
    return result;
}

class TestRuntimeService final : public IRuntimeService
{
public:
    TestRuntimeService(RuntimeServiceDescriptor descriptor, ServiceCallLog* log)
        : m_descriptor(std::move(descriptor))
        , m_log(log)
    {
    }

    const RuntimeServiceDescriptor& Descriptor() const noexcept override
    {
        return m_descriptor;
    }

    RuntimeServiceLifecycleResult Start() override
    {
        ++startCount;
        Record("start");
        return startResult;
    }

    RuntimeServiceLifecycleResult Tick() override
    {
        ++tickCount;
        Record("tick");
        return tickResult;
    }

    RuntimeServiceLifecycleResult Stop() override
    {
        ++stopCount;
        Record("stop");
        return stopResult;
    }

    int startCount = 0;
    int tickCount = 0;
    int stopCount = 0;
    RuntimeServiceLifecycleResult startResult;
    RuntimeServiceLifecycleResult tickResult;
    RuntimeServiceLifecycleResult stopResult;

private:
    void Record(const std::string& operation)
    {
        if (m_log)
        {
            m_log->entries.push_back(operation + ":" + m_descriptor.serviceId);
        }
    }

    RuntimeServiceDescriptor m_descriptor;
    ServiceCallLog* m_log = nullptr;
};

[[nodiscard]] std::unique_ptr<TestRuntimeService> MakeService(
    const std::string& serviceId,
    ServiceCallLog* log = nullptr)
{
    RuntimeServiceDescriptor descriptor = DescriptorFor(serviceId);
    auto service = std::make_unique<TestRuntimeService>(descriptor, log);
    service->startResult = ResultFor(descriptor, RuntimeServiceState::Started);
    service->tickResult = ResultFor(descriptor, RuntimeServiceState::Started);
    service->stopResult = ResultFor(descriptor, RuntimeServiceState::Stopped);
    return service;
}

[[nodiscard]] RuntimeServiceDiagnostic ErrorDiagnostic(
    const std::string& serviceId,
    const std::string& message)
{
    return RuntimeServiceLifecycle::Diagnostic(
        RuntimeServiceDiagnosticSeverity::Error,
        serviceId,
        message);
}

} // namespace

TEST(RuntimeServiceRegistryTests, EmptyRegistryOperationsSucceed)
{
    RuntimeServiceRegistry registry;

    EXPECT_TRUE(registry.StartAll().Succeeded());
    EXPECT_TRUE(registry.TickStarted().Succeeded());
    EXPECT_TRUE(registry.StopAll().Succeeded());
    EXPECT_EQ(registry.ServiceCount(), 0U);
}

TEST(RuntimeServiceRegistryTests, InvalidDescriptorIsRejected)
{
    RuntimeServiceRegistry registry;
    RuntimeServiceDescriptor descriptor;
    descriptor.serviceId = "runtime.invalid";
    auto service = std::make_unique<TestRuntimeService>(descriptor, nullptr);

    const RuntimeServiceLifecycleResult result =
        registry.Register(std::move(service));

    EXPECT_FALSE(result.Succeeded());
    EXPECT_EQ(result.state, RuntimeServiceState::Failed);
    EXPECT_EQ(registry.ServiceCount(), 0U);
    ASSERT_EQ(result.diagnostics.size(), 1U);
    EXPECT_EQ(result.diagnostics.front().serviceId, "runtime.invalid");
    EXPECT_EQ(result.diagnostics.front().severity,
              RuntimeServiceDiagnosticSeverity::Error);
}

TEST(RuntimeServiceRegistryTests, DuplicateServiceIdIsRejected)
{
    RuntimeServiceRegistry registry;

    EXPECT_TRUE(registry.Register(MakeService("runtime.alpha")).Succeeded());
    const RuntimeServiceLifecycleResult duplicate =
        registry.Register(MakeService("runtime.alpha"));

    EXPECT_FALSE(duplicate.Succeeded());
    EXPECT_EQ(registry.ServiceCount(), 1U);
    ASSERT_EQ(duplicate.diagnostics.size(), 1U);
    EXPECT_EQ(duplicate.diagnostics.front().serviceId, "runtime.alpha");
}

TEST(RuntimeServiceRegistryTests, StartOrderIsDeterministic)
{
    RuntimeServiceRegistry registry;
    ServiceCallLog log;

    EXPECT_TRUE(registry.Register(MakeService("runtime.alpha", &log)).Succeeded());
    EXPECT_TRUE(registry.Register(MakeService("runtime.beta", &log)).Succeeded());
    EXPECT_TRUE(registry.Register(MakeService("runtime.gamma", &log)).Succeeded());

    const auto report = registry.StartAll();

    EXPECT_TRUE(report.Succeeded());
    EXPECT_EQ(log.entries,
              (std::vector<std::string>{
                  "start:runtime.alpha",
                  "start:runtime.beta",
                  "start:runtime.gamma",
              }));
}

TEST(RuntimeServiceRegistryTests, TickOnlyStartedServices)
{
    RuntimeServiceRegistry registry;
    ServiceCallLog log;
    auto alpha = MakeService("runtime.alpha", &log);
    auto beta = MakeService("runtime.beta", &log);
    beta->startResult = ResultFor(
        DescriptorFor("runtime.beta"),
        RuntimeServiceState::Failed,
        {ErrorDiagnostic("runtime.beta", "beta failed")});

    EXPECT_TRUE(registry.Register(std::move(alpha)).Succeeded());
    EXPECT_TRUE(registry.Register(std::move(beta)).Succeeded());

    EXPECT_FALSE(registry.StartAll().Succeeded());
    log.entries.clear();

    const auto tickReport = registry.TickStarted();

    EXPECT_TRUE(tickReport.Succeeded());
    EXPECT_EQ(log.entries,
              (std::vector<std::string>{"tick:runtime.alpha"}));
}

TEST(RuntimeServiceRegistryTests, FailedStartBlocksLaterServices)
{
    RuntimeServiceRegistry registry;
    ServiceCallLog log;
    auto beta = MakeService("runtime.beta", &log);
    beta->startResult = ResultFor(
        DescriptorFor("runtime.beta"),
        RuntimeServiceState::Failed,
        {ErrorDiagnostic("runtime.beta", "beta failed")});

    EXPECT_TRUE(registry.Register(MakeService("runtime.alpha", &log)).Succeeded());
    EXPECT_TRUE(registry.Register(std::move(beta)).Succeeded());
    EXPECT_TRUE(registry.Register(MakeService("runtime.gamma", &log)).Succeeded());

    const auto report = registry.StartAll();

    EXPECT_FALSE(report.Succeeded());
    EXPECT_EQ(log.entries,
              (std::vector<std::string>{
                  "start:runtime.alpha",
                  "start:runtime.beta",
              }));
    ASSERT_TRUE(registry.GetState("runtime.alpha").has_value());
    EXPECT_EQ(*registry.GetState("runtime.alpha"), RuntimeServiceState::Started);
    ASSERT_TRUE(registry.GetState("runtime.beta").has_value());
    EXPECT_EQ(*registry.GetState("runtime.beta"), RuntimeServiceState::Failed);
    ASSERT_TRUE(registry.GetState("runtime.gamma").has_value());
    EXPECT_EQ(*registry.GetState("runtime.gamma"), RuntimeServiceState::Registered);
}

TEST(RuntimeServiceRegistryTests, StopAllIsReverseOrderAndIdempotent)
{
    RuntimeServiceRegistry registry;
    ServiceCallLog log;

    EXPECT_TRUE(registry.Register(MakeService("runtime.alpha", &log)).Succeeded());
    EXPECT_TRUE(registry.Register(MakeService("runtime.beta", &log)).Succeeded());
    EXPECT_TRUE(registry.StartAll().Succeeded());
    log.entries.clear();

    EXPECT_TRUE(registry.StopAll().Succeeded());
    EXPECT_EQ(log.entries,
              (std::vector<std::string>{
                  "stop:runtime.beta",
                  "stop:runtime.alpha",
              }));

    log.entries.clear();
    EXPECT_TRUE(registry.StopAll().Succeeded());
    EXPECT_TRUE(log.entries.empty());
}

TEST(RuntimeServiceRegistryTests, DiagnosticsPreserveServiceIdMessageAndSeverity)
{
    RuntimeServiceRegistry registry;
    auto service = MakeService("runtime.alpha");
    service->tickResult = ResultFor(
        DescriptorFor("runtime.alpha"),
        RuntimeServiceState::Started,
        {RuntimeServiceLifecycle::Diagnostic(
            RuntimeServiceDiagnosticSeverity::Warning,
            "runtime.alpha",
            "alpha warning")});

    EXPECT_TRUE(registry.Register(std::move(service)).Succeeded());
    EXPECT_TRUE(registry.StartAll().Succeeded());

    const auto report = registry.TickStarted();

    EXPECT_TRUE(report.Succeeded());
    ASSERT_EQ(report.diagnostics.size(), 1U);
    EXPECT_EQ(report.diagnostics.front().serviceId, "runtime.alpha");
    EXPECT_EQ(report.diagnostics.front().message, "alpha warning");
    EXPECT_EQ(report.diagnostics.front().severity,
              RuntimeServiceDiagnosticSeverity::Warning);
}

TEST(RuntimeServiceRegistryTests, DescriptorsRemainInRegistrationOrder)
{
    RuntimeServiceRegistry registry;

    EXPECT_TRUE(registry.Register(MakeService("runtime.alpha")).Succeeded());
    EXPECT_TRUE(registry.Register(MakeService("runtime.beta")).Succeeded());

    const auto descriptors = registry.GetDescriptorsInOrder();

    ASSERT_EQ(descriptors.size(), 2U);
    EXPECT_EQ(descriptors[0].serviceId, "runtime.alpha");
    EXPECT_EQ(descriptors[1].serviceId, "runtime.beta");
}
