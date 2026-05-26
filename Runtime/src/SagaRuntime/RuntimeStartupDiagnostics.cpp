/// @file RuntimeStartupDiagnostics.cpp
/// @brief Runtime startup report diagnostic classification implementation.

#include "SagaRuntime/RuntimeStartupDiagnostics.hpp"

namespace SagaRuntime
{
namespace
{

[[nodiscard]] RuntimeStartupDiagnosticView ToView(
    const RuntimeStartupPreflightDiagnostic& diagnostic)
{
    RuntimeStartupDiagnosticView view;
    view.severity = RuntimeStartupDiagnosticSeverity::Error;
    view.category = RuntimeStartupDiagnosticCategory::StartupPreflight;
    view.blocking = true;
    view.diagnosticId = diagnostic.diagnosticId;
    view.message = diagnostic.message;
    view.manifestPath = diagnostic.manifestPath;
    view.fieldName = diagnostic.fieldName;
    view.referenceIndex = diagnostic.referenceIndex;
    view.itemIndex = diagnostic.itemIndex;
    view.resourceId = diagnostic.resourceId;
    view.resolvedPath = diagnostic.resolvedPath;
    return view;
}

} // namespace

RuntimeStartupReportSummary RuntimeStartupDiagnostics::Summarize(
    const RuntimeStartupReport& report)
{
    RuntimeStartupReportSummary summary;
    summary.preflightSkipped = report.preflight.validationSkipped;
    summary.diagnosticCount = report.preflight.diagnostics.size();

    if (report.preflight.validationSkipped)
    {
        summary.state = RuntimeStartupReportState::PreflightSkipped;
        summary.startupAllowed = true;
        summary.blockingDiagnosticCount = 0;
        return summary;
    }

    if (!report.preflight.diagnostics.empty())
    {
        summary.state = RuntimeStartupReportState::Blocked;
        summary.startupAllowed = false;
        summary.blockingDiagnosticCount = report.preflight.diagnostics.size();
        return summary;
    }

    summary.state = RuntimeStartupReportState::Ready;
    summary.startupAllowed = true;
    summary.blockingDiagnosticCount = 0;
    return summary;
}

std::vector<RuntimeStartupDiagnosticView>
RuntimeStartupDiagnostics::BuildDiagnosticViews(
    const RuntimeStartupReport& report)
{
    std::vector<RuntimeStartupDiagnosticView> views;
    views.reserve(report.preflight.diagnostics.size());
    for (const RuntimeStartupPreflightDiagnostic& diagnostic :
         report.preflight.diagnostics)
    {
        views.push_back(ToView(diagnostic));
    }
    return views;
}

} // namespace SagaRuntime
