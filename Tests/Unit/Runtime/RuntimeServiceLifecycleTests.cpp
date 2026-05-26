/// @file RuntimeServiceLifecycleTests.cpp
/// @brief Focused tests for Runtime service lifecycle value semantics.

#include <SagaRuntime/RuntimeServiceLifecycle.hpp>

#include <gtest/gtest.h>

#include <string>
#include <vector>

namespace
{

using SagaRuntime::RuntimeServiceDescriptor;
using SagaRuntime::RuntimeServiceDiagnosticSeverity;
using SagaRuntime::RuntimeServiceLifecycle;
using SagaRuntime::RuntimeServiceLifecycleResult;
using SagaRuntime::RuntimeServiceState;

[[nodiscard]] RuntimeServiceDescriptor MakeDescriptor()
{
    RuntimeServiceDescriptor descriptor;
    descriptor.serviceId = "runtime.test";
    descriptor.displayName = "Runtime Test Service";
    descriptor.category = "test";
    return descriptor;
}

} // namespace

TEST(RuntimeServiceLifecycleTests, DescriptorRequiresStableIdAndName)
{
    EXPECT_TRUE(MakeDescriptor().IsValid());

    RuntimeServiceDescriptor missingId = MakeDescriptor();
    missingId.serviceId.clear();
    EXPECT_FALSE(missingId.IsValid());

    RuntimeServiceDescriptor missingName = MakeDescriptor();
    missingName.displayName = "   ";
    EXPECT_FALSE(missingName.IsValid());
}

TEST(RuntimeServiceLifecycleTests, SuccessResultIgnoresInformationalDiagnostics)
{
    std::vector<SagaRuntime::RuntimeServiceDiagnostic> diagnostics;
    diagnostics.push_back(RuntimeServiceLifecycle::Diagnostic(
        RuntimeServiceDiagnosticSeverity::Info,
        "runtime.test",
        "service started"));

    const RuntimeServiceLifecycleResult result =
        RuntimeServiceLifecycle::Transition(MakeDescriptor(),
                                            RuntimeServiceState::Registered,
                                            RuntimeServiceState::Started,
                                            diagnostics);

    EXPECT_TRUE(result.Succeeded());
    EXPECT_EQ(result.state, RuntimeServiceState::Started);
    ASSERT_EQ(result.diagnostics.size(), 1U);
    EXPECT_EQ(result.diagnostics.front().message, "service started");
}

TEST(RuntimeServiceLifecycleTests, ErrorDiagnosticFailsResult)
{
    std::vector<SagaRuntime::RuntimeServiceDiagnostic> diagnostics;
    diagnostics.push_back(RuntimeServiceLifecycle::Diagnostic(
        RuntimeServiceDiagnosticSeverity::Error,
        "runtime.test",
        "service failed"));

    const RuntimeServiceLifecycleResult result =
        RuntimeServiceLifecycle::Transition(MakeDescriptor(),
                                            RuntimeServiceState::Registered,
                                            RuntimeServiceState::Started,
                                            diagnostics);

    EXPECT_FALSE(result.Succeeded());
    EXPECT_EQ(result.state, RuntimeServiceState::Started);
    ASSERT_EQ(result.diagnostics.size(), 1U);
    EXPECT_EQ(result.diagnostics.front().serviceId, "runtime.test");
    EXPECT_EQ(result.diagnostics.front().message, "service failed");
}

TEST(RuntimeServiceLifecycleTests, StartAndStopTransitionsAreAccepted)
{
    EXPECT_TRUE(RuntimeServiceLifecycle::CanTransition(
        RuntimeServiceState::Registered,
        RuntimeServiceState::Started));
    EXPECT_TRUE(RuntimeServiceLifecycle::CanTransition(
        RuntimeServiceState::Started,
        RuntimeServiceState::Stopped));

    const RuntimeServiceLifecycleResult started =
        RuntimeServiceLifecycle::Transition(MakeDescriptor(),
                                            RuntimeServiceState::Registered,
                                            RuntimeServiceState::Started);
    EXPECT_TRUE(started.Succeeded());
    EXPECT_EQ(started.state, RuntimeServiceState::Started);

    const RuntimeServiceLifecycleResult stopped =
        RuntimeServiceLifecycle::Transition(MakeDescriptor(),
                                            RuntimeServiceState::Started,
                                            RuntimeServiceState::Stopped);
    EXPECT_TRUE(stopped.Succeeded());
    EXPECT_EQ(stopped.state, RuntimeServiceState::Stopped);
}

TEST(RuntimeServiceLifecycleTests, FailedServiceCannotRestart)
{
    EXPECT_FALSE(RuntimeServiceLifecycle::CanTransition(
        RuntimeServiceState::Failed,
        RuntimeServiceState::Started));

    const RuntimeServiceLifecycleResult result =
        RuntimeServiceLifecycle::Transition(MakeDescriptor(),
                                            RuntimeServiceState::Failed,
                                            RuntimeServiceState::Started);

    EXPECT_FALSE(result.Succeeded());
    EXPECT_EQ(result.state, RuntimeServiceState::Failed);
    ASSERT_EQ(result.diagnostics.size(), 1U);
    EXPECT_EQ(result.diagnostics.front().severity,
              RuntimeServiceDiagnosticSeverity::Error);
}

TEST(RuntimeServiceLifecycleTests, StopAndFailureTransitionsAreIdempotent)
{
    EXPECT_TRUE(RuntimeServiceLifecycle::CanTransition(
        RuntimeServiceState::Stopped,
        RuntimeServiceState::Stopped));
    EXPECT_TRUE(RuntimeServiceLifecycle::CanTransition(
        RuntimeServiceState::Failed,
        RuntimeServiceState::Failed));

    const RuntimeServiceLifecycleResult stopped =
        RuntimeServiceLifecycle::Transition(MakeDescriptor(),
                                            RuntimeServiceState::Stopped,
                                            RuntimeServiceState::Stopped);
    EXPECT_TRUE(stopped.Succeeded());
    EXPECT_EQ(stopped.state, RuntimeServiceState::Stopped);
}

TEST(RuntimeServiceLifecycleTests, InvalidDescriptorProducesBlockingDiagnostic)
{
    RuntimeServiceDescriptor descriptor;
    descriptor.serviceId = "runtime.invalid";

    const RuntimeServiceLifecycleResult result =
        RuntimeServiceLifecycle::Transition(descriptor,
                                            RuntimeServiceState::Registered,
                                            RuntimeServiceState::Started);

    EXPECT_FALSE(result.Succeeded());
    EXPECT_EQ(result.state, RuntimeServiceState::Failed);
    ASSERT_EQ(result.diagnostics.size(), 1U);
    EXPECT_EQ(result.diagnostics.front().serviceId, "runtime.invalid");
    EXPECT_EQ(result.diagnostics.front().severity,
              RuntimeServiceDiagnosticSeverity::Error);
}
