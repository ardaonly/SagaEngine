/// @file RuntimeServiceRegistryDiagnosticsTests.cpp
/// @brief Focused tests for Runtime service registry diagnostic classification.

#include <SagaRuntime/RuntimeServiceRegistryDiagnostics.hpp>

#include <gtest/gtest.h>

#include <array>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <utility>

namespace
{

using SagaRuntime::RuntimeServiceDescriptor;
using SagaRuntime::RuntimeServiceDiagnostic;
using SagaRuntime::RuntimeServiceDiagnosticSeverity;
using SagaRuntime::RuntimeServiceLifecycle;
using SagaRuntime::RuntimeServiceLifecycleResult;
using SagaRuntime::RuntimeServiceRegistryDiagnostics;
using SagaRuntime::RuntimeServiceRegistryReport;
using SagaRuntime::RuntimeServiceRegistryReportState;
using SagaRuntime::RuntimeServiceState;

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
    RuntimeServiceState state)
{
    RuntimeServiceLifecycleResult result;
    result.descriptor = std::move(descriptor);
    result.state = state;
    return result;
}

[[nodiscard]] RuntimeServiceDiagnostic Diagnostic(
    RuntimeServiceDiagnosticSeverity severity,
    const std::string& serviceId,
    const std::string& message)
{
    return RuntimeServiceLifecycle::Diagnostic(severity, serviceId, message);
}

[[nodiscard]] bool IsRuntimeSourceFile(const std::filesystem::path& path)
{
    return path.extension() == ".hpp" || path.extension() == ".cpp";
}

[[nodiscard]] std::string ReadTextFile(const std::filesystem::path& path)
{
    std::ifstream file(path);
    return std::string(std::istreambuf_iterator<char>(file),
                       std::istreambuf_iterator<char>());
}

} // namespace

TEST(RuntimeServiceRegistryDiagnosticsTests,
     EmptySuccessfulReportSummarizesAsIdle)
{
    RuntimeServiceRegistryReport report;

    const auto summary = RuntimeServiceRegistryDiagnostics::Summarize(report);

    EXPECT_EQ(summary.state, RuntimeServiceRegistryReportState::Idle);
    EXPECT_TRUE(summary.operationSucceeded);
    EXPECT_EQ(summary.serviceResultCount, 0U);
    EXPECT_EQ(summary.diagnosticCount, 0U);
    EXPECT_EQ(summary.errorDiagnosticCount, 0U);
    EXPECT_EQ(summary.warningDiagnosticCount, 0U);
}

TEST(RuntimeServiceRegistryDiagnosticsTests,
     SuccessfulReportWithStartedServiceSummarizesAsReady)
{
    RuntimeServiceRegistryReport report;
    report.results.push_back(
        ResultFor(DescriptorFor("runtime.alpha"), RuntimeServiceState::Started));

    const auto summary = RuntimeServiceRegistryDiagnostics::Summarize(report);

    EXPECT_EQ(summary.state, RuntimeServiceRegistryReportState::Ready);
    EXPECT_TRUE(summary.operationSucceeded);
    EXPECT_EQ(summary.serviceResultCount, 1U);
}

TEST(RuntimeServiceRegistryDiagnosticsTests,
     FailedLifecycleResultSummarizesAsBlocked)
{
    RuntimeServiceRegistryReport report;
    report.results.push_back(
        ResultFor(DescriptorFor("runtime.alpha"), RuntimeServiceState::Failed));

    const auto summary = RuntimeServiceRegistryDiagnostics::Summarize(report);

    EXPECT_EQ(summary.state, RuntimeServiceRegistryReportState::Blocked);
    EXPECT_FALSE(summary.operationSucceeded);
    EXPECT_EQ(summary.serviceResultCount, 1U);
}

TEST(RuntimeServiceRegistryDiagnosticsTests,
     ErrorDiagnosticSummarizesAsBlocked)
{
    RuntimeServiceRegistryReport report;
    report.diagnostics.push_back(Diagnostic(RuntimeServiceDiagnosticSeverity::Error,
                                            "runtime.alpha",
                                            "alpha failed"));

    const auto summary = RuntimeServiceRegistryDiagnostics::Summarize(report);

    EXPECT_EQ(summary.state, RuntimeServiceRegistryReportState::Blocked);
    EXPECT_FALSE(summary.operationSucceeded);
    EXPECT_EQ(summary.diagnosticCount, 1U);
    EXPECT_EQ(summary.errorDiagnosticCount, 1U);
    EXPECT_EQ(summary.warningDiagnosticCount, 0U);
}

TEST(RuntimeServiceRegistryDiagnosticsTests,
     WarningAndInfoDiagnosticsDoNotBlockSuccess)
{
    RuntimeServiceRegistryReport report;
    report.results.push_back(
        ResultFor(DescriptorFor("runtime.alpha"), RuntimeServiceState::Started));
    report.diagnostics.push_back(Diagnostic(RuntimeServiceDiagnosticSeverity::Warning,
                                            "runtime.alpha",
                                            "alpha warning"));
    report.diagnostics.push_back(Diagnostic(RuntimeServiceDiagnosticSeverity::Info,
                                            "runtime.beta",
                                            "beta info"));

    const auto summary = RuntimeServiceRegistryDiagnostics::Summarize(report);

    EXPECT_EQ(summary.state, RuntimeServiceRegistryReportState::Ready);
    EXPECT_TRUE(summary.operationSucceeded);
    EXPECT_EQ(summary.diagnosticCount, 2U);
    EXPECT_EQ(summary.errorDiagnosticCount, 0U);
    EXPECT_EQ(summary.warningDiagnosticCount, 1U);
}

TEST(RuntimeServiceRegistryDiagnosticsTests,
     DiagnosticViewsPreserveSeverityServiceIdAndMessage)
{
    RuntimeServiceRegistryReport report;
    report.diagnostics.push_back(Diagnostic(RuntimeServiceDiagnosticSeverity::Warning,
                                            "runtime.alpha",
                                            "alpha warning"));
    report.diagnostics.push_back(Diagnostic(RuntimeServiceDiagnosticSeverity::Error,
                                            "runtime.beta",
                                            "beta failed"));

    const auto views =
        RuntimeServiceRegistryDiagnostics::BuildDiagnosticViews(report);

    ASSERT_EQ(views.size(), 2U);
    EXPECT_EQ(views[0].severity, RuntimeServiceDiagnosticSeverity::Warning);
    EXPECT_EQ(views[0].serviceId, "runtime.alpha");
    EXPECT_EQ(views[0].message, "alpha warning");
    EXPECT_EQ(views[1].severity, RuntimeServiceDiagnosticSeverity::Error);
    EXPECT_EQ(views[1].serviceId, "runtime.beta");
    EXPECT_EQ(views[1].message, "beta failed");
}

TEST(RuntimeServiceRegistryDiagnosticsTests,
     RuntimeSourcesDoNotReferenceClientHostTypes)
{
    constexpr std::array<const char*, 3> forbiddenTokens = {
        "ClientHost",
        "ClientConfig",
        "Saga::ClientConfig",
    };

    const std::filesystem::path root =
        std::filesystem::path(SAGA_SOURCE_ROOT) / "Engine/Source/Runtime";
    ASSERT_TRUE(std::filesystem::is_directory(root));
    for (const std::filesystem::directory_entry& entry :
         std::filesystem::recursive_directory_iterator(root))
    {
        if (!entry.is_regular_file() || !IsRuntimeSourceFile(entry.path()))
        {
            continue;
        }
        const std::string contents = ReadTextFile(entry.path());
        for (const char* token : forbiddenTokens)
        {
            EXPECT_EQ(contents.find(token), std::string::npos)
                << entry.path() << " references " << token;
        }
    }
}
