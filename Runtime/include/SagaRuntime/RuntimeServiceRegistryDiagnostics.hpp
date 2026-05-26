/// @file RuntimeServiceRegistryDiagnostics.hpp
/// @brief Runtime-owned service registry report diagnostic classification.

#pragma once

#include <SagaRuntime/RuntimeServiceRegistry.hpp>

#include <cstddef>
#include <string>
#include <vector>

namespace SagaRuntime
{

/// Classified Runtime service registry report state.
enum class RuntimeServiceRegistryReportState
{
    Ready = 0,
    Blocked,
    Idle,
};

/// Summary of a Runtime service registry lifecycle report.
struct RuntimeServiceRegistryReportSummary
{
    RuntimeServiceRegistryReportState state =
        RuntimeServiceRegistryReportState::Idle;
    bool operationSucceeded = true;
    std::size_t serviceResultCount = 0;
    std::size_t diagnosticCount = 0;
    std::size_t errorDiagnosticCount = 0;
    std::size_t warningDiagnosticCount = 0;
};

/// Classified view over one Runtime service registry diagnostic.
struct RuntimeServiceDiagnosticView
{
    RuntimeServiceDiagnosticSeverity severity =
        RuntimeServiceDiagnosticSeverity::Error;
    std::string serviceId;
    std::string message;
};

/// Runtime-owned service registry report classifier.
class RuntimeServiceRegistryDiagnostics
{
public:
    /// Summarize registry lifecycle state without owning app logging or exit policy.
    [[nodiscard]] static RuntimeServiceRegistryReportSummary Summarize(
        const RuntimeServiceRegistryReport& report);

    /// Build diagnostic views for app-owned logging.
    [[nodiscard]] static std::vector<RuntimeServiceDiagnosticView>
    BuildDiagnosticViews(const RuntimeServiceRegistryReport& report);
};

} // namespace SagaRuntime
