/// @file RuntimeAssetBootstrapDiagnostics.cpp
/// @brief Runtime asset bootstrap diagnostic classification implementation.

#include "SagaRuntime/RuntimeAssetBootstrapDiagnostics.hpp"

namespace SagaRuntime
{
namespace
{

[[nodiscard]] bool IsWarning(
    RuntimeAssetBootstrapDiagnosticSeverity severity) noexcept
{
    switch (severity)
    {
        case RuntimeAssetBootstrapDiagnosticSeverity::Error:
            return false;
    }

    return false;
}

[[nodiscard]] bool IsError(
    RuntimeAssetBootstrapDiagnosticSeverity severity) noexcept
{
    switch (severity)
    {
        case RuntimeAssetBootstrapDiagnosticSeverity::Error:
            return true;
    }

    return true;
}

[[nodiscard]] RuntimeAssetBootstrapDiagnosticView ToView(
    const RuntimeAssetBootstrapDiagnostic& diagnostic)
{
    RuntimeAssetBootstrapDiagnosticView view;
    view.severity = diagnostic.severity;
    view.category = diagnostic.category;
    view.blocking = IsError(diagnostic.severity);
    view.diagnosticId = diagnostic.diagnosticId;
    view.message = diagnostic.message;
    view.manifestPath = diagnostic.manifestPath;
    view.fieldName = diagnostic.fieldName;
    view.referenceIndex = diagnostic.referenceIndex;
    view.itemIndex = diagnostic.itemIndex;
    view.resourceId = diagnostic.resourceId;
    view.assetId = diagnostic.assetId;
    view.resolvedPath = diagnostic.resolvedPath;
    return view;
}

void CountDiagnostics(
    RuntimeAssetBootstrapReportSummary& summary,
    const RuntimeAssetBootstrapResult& result)
{
    summary.diagnosticCount = result.diagnostics.size();
    for (const RuntimeAssetBootstrapDiagnostic& diagnostic :
         result.diagnostics)
    {
        if (IsError(diagnostic.severity))
        {
            ++summary.errorCount;
        }
        else if (IsWarning(diagnostic.severity))
        {
            ++summary.warningCount;
        }
    }
}

} // namespace

RuntimeAssetBootstrapReportSummary
RuntimeAssetBootstrapDiagnostics::Summarize(
    const RuntimeAssetBootstrapResult& result)
{
    RuntimeAssetBootstrapReportSummary summary;
    summary.succeeded = result.Succeeded();
    summary.registeredAssetCount = result.registeredAssetCount;
    CountDiagnostics(summary, result);

    if (summary.errorCount > 0)
    {
        summary.state = RuntimeAssetBootstrapReportState::Blocked;
        summary.succeeded = false;
        return summary;
    }

    if (result.registeredAssetCount == 0)
    {
        summary.state = RuntimeAssetBootstrapReportState::Empty;
        return summary;
    }

    summary.state = RuntimeAssetBootstrapReportState::Ready;
    return summary;
}

RuntimeAssetBootstrapReportSummary
RuntimeAssetBootstrapDiagnostics::Summarize(
    const RuntimeAssetBootstrapResult& result,
    const RuntimeAssetBootstrapOptions& options)
{
    RuntimeAssetBootstrapReportSummary summary = Summarize(result);
    summary.packageManifestPath = options.packageManifestPath;
    summary.packageBaseDirectory = options.packageBaseDirectory;
    return summary;
}

std::vector<RuntimeAssetBootstrapDiagnosticView>
RuntimeAssetBootstrapDiagnostics::BuildDiagnosticViews(
    const RuntimeAssetBootstrapResult& result)
{
    std::vector<RuntimeAssetBootstrapDiagnosticView> views;
    views.reserve(result.diagnostics.size());
    for (const RuntimeAssetBootstrapDiagnostic& diagnostic :
         result.diagnostics)
    {
        views.push_back(ToView(diagnostic));
    }
    return views;
}

} // namespace SagaRuntime
