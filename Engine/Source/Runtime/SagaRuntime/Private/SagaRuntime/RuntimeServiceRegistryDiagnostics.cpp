/// @file RuntimeServiceRegistryDiagnostics.cpp
/// @brief Runtime service registry report diagnostic classification implementation.

#include "SagaRuntime/RuntimeServiceRegistryDiagnostics.hpp"

#include <algorithm>

namespace SagaRuntime
{
namespace
{

[[nodiscard]] bool IsErrorDiagnostic(
    const RuntimeServiceDiagnostic& diagnostic) noexcept
{
    return diagnostic.severity == RuntimeServiceDiagnosticSeverity::Error;
}

[[nodiscard]] bool IsWarningDiagnostic(
    const RuntimeServiceDiagnostic& diagnostic) noexcept
{
    return diagnostic.severity == RuntimeServiceDiagnosticSeverity::Warning;
}

[[nodiscard]] RuntimeServiceDiagnosticView ToView(
    const RuntimeServiceDiagnostic& diagnostic)
{
    RuntimeServiceDiagnosticView view;
    view.severity = diagnostic.severity;
    view.serviceId = diagnostic.serviceId;
    view.message = diagnostic.message;
    return view;
}

} // namespace

RuntimeServiceRegistryReportSummary RuntimeServiceRegistryDiagnostics::Summarize(
    const RuntimeServiceRegistryReport& report)
{
    RuntimeServiceRegistryReportSummary summary;
    summary.operationSucceeded = report.Succeeded();
    summary.serviceResultCount = report.results.size();
    summary.diagnosticCount = report.diagnostics.size();
    summary.errorDiagnosticCount = static_cast<std::size_t>(std::count_if(
        report.diagnostics.begin(),
        report.diagnostics.end(),
        IsErrorDiagnostic));
    summary.warningDiagnosticCount = static_cast<std::size_t>(std::count_if(
        report.diagnostics.begin(),
        report.diagnostics.end(),
        IsWarningDiagnostic));

    if (!summary.operationSucceeded || summary.errorDiagnosticCount > 0U)
    {
        summary.state = RuntimeServiceRegistryReportState::Blocked;
        return summary;
    }

    if (summary.serviceResultCount == 0U)
    {
        summary.state = RuntimeServiceRegistryReportState::Idle;
        return summary;
    }

    summary.state = RuntimeServiceRegistryReportState::Ready;
    return summary;
}

std::vector<RuntimeServiceDiagnosticView>
RuntimeServiceRegistryDiagnostics::BuildDiagnosticViews(
    const RuntimeServiceRegistryReport& report)
{
    std::vector<RuntimeServiceDiagnosticView> views;
    views.reserve(report.diagnostics.size());
    for (const RuntimeServiceDiagnostic& diagnostic : report.diagnostics)
    {
        views.push_back(ToView(diagnostic));
    }
    return views;
}

} // namespace SagaRuntime
