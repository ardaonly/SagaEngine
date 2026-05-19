/// @file BuildReportBuilder.cpp
/// @brief Build report creation from BuildPlan validation.

#include "Forge/Reports/BuildReportBuilder.hpp"

#include "Forge/Diagnostics/DiagnosticCollector.hpp"

#include <utility>

namespace Forge::Reports
{

namespace
{

std::string IssueKindName(const BuildPlanIssueKind kind)
{
    switch (kind)
    {
        case BuildPlanIssueKind::DuplicateStepId:
            return "DuplicateStepId";
        case BuildPlanIssueKind::MissingDependency:
            return "MissingDependency";
        case BuildPlanIssueKind::DependencyCycle:
            return "DependencyCycle";
        case BuildPlanIssueKind::DuplicateOutputWriter:
            return "DuplicateOutputWriter";
    }

    return "Unknown";
}

std::string IssueCode(const BuildPlanIssueKind kind)
{
    return "Forge.BuildPlan." + IssueKindName(kind);
}

std::string IssueTitle(const BuildPlanIssueKind kind)
{
    switch (kind)
    {
        case BuildPlanIssueKind::DuplicateStepId:
            return "Duplicate build plan step id";
        case BuildPlanIssueKind::MissingDependency:
            return "Missing build plan dependency";
        case BuildPlanIssueKind::DependencyCycle:
            return "Build plan dependency cycle";
        case BuildPlanIssueKind::DuplicateOutputWriter:
            return "Duplicate build plan output writer";
    }

    return "Build plan validation issue";
}

std::string IssueMessage(const BuildPlanValidationIssue& issue)
{
    switch (issue.kind)
    {
        case BuildPlanIssueKind::DuplicateStepId:
            return "Step '" + issue.stepId + "' duplicates an existing step id.";
        case BuildPlanIssueKind::MissingDependency:
            return "Step '" + issue.stepId + "' depends on missing step '" +
                   issue.reference + "'.";
        case BuildPlanIssueKind::DependencyCycle:
            return "Step '" + issue.stepId + "' participates in a dependency cycle with '" +
                   issue.reference + "'.";
        case BuildPlanIssueKind::DuplicateOutputWriter:
            return "Step '" + issue.stepId + "' writes output '" + issue.reference +
                   "' that is already written by another step.";
    }

    return "Build plan validation failed.";
}

BuildReportStep MakeReportStep(const BuildStep& step)
{
    BuildReportStep reportStep;
    reportStep.id = step.id;
    reportStep.kind = step.kind;
    reportStep.inputs = step.inputs;
    reportStep.outputs = step.outputs;
    reportStep.dependencies = step.dependencies;
    reportStep.toolName = step.toolName;
    reportStep.failurePolicy = step.failurePolicy;
    return reportStep;
}

Diagnostics::ForgeDiagnostic MakeDiagnostic(const BuildPlanValidationIssue& issue)
{
    Diagnostics::ForgeDiagnostic diagnostic;
    diagnostic.severity = Diagnostics::ForgeDiagnosticSeverity::BuildBlocking;
    diagnostic.code = IssueCode(issue.kind);
    diagnostic.title = IssueTitle(issue.kind);
    diagnostic.message = IssueMessage(issue);
    diagnostic.stepId = issue.stepId;
    diagnostic.reference = issue.reference;
    diagnostic.metadata.emplace("issueKind", IssueKindName(issue.kind));
    return diagnostic;
}

} // namespace

BuildReport BuildReportBuilder::CreatePlannedReport(
    const BuildPlan& plan,
    const BuildPlanValidationResult& validation)
{
    BuildReport report;
    report.status = validation.IsValid()
                        ? BuildReportStatus::Planned
                        : BuildReportStatus::Blocked;
    report.metadata.emplace("forge.report.kind", "build");
    report.metadata.emplace("forge.report.version", "1");

    for (const BuildStep& step : plan.steps)
    {
        report.steps.push_back(MakeReportStep(step));
    }

    Diagnostics::DiagnosticCollector diagnostics;
    for (const BuildPlanValidationIssue& issue : validation.issues)
    {
        diagnostics.Add(MakeDiagnostic(issue));
    }
    diagnostics.AppendTo(report);

    report.summary.stepCount = report.steps.size();
    report.summary.diagnosticCount = diagnostics.Count();
    report.summary.buildBlockingCount = diagnostics.BuildBlockingCount();

    return report;
}

} // namespace Forge::Reports
