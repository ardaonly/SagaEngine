/// @file DiagnosticCollector.cpp
/// @brief Forge diagnostic collection helpers.

#include "Forge/Diagnostics/DiagnosticCollector.hpp"

#include <utility>

namespace Forge::Diagnostics
{

namespace
{

Reports::BuildReportDiagnosticSeverity ToReportSeverity(
    const ForgeDiagnosticSeverity severity)
{
    switch (severity)
    {
        case ForgeDiagnosticSeverity::Info:
            return Reports::BuildReportDiagnosticSeverity::Info;
        case ForgeDiagnosticSeverity::Warning:
            return Reports::BuildReportDiagnosticSeverity::Warning;
        case ForgeDiagnosticSeverity::Error:
            return Reports::BuildReportDiagnosticSeverity::Error;
        case ForgeDiagnosticSeverity::BuildBlocking:
            return Reports::BuildReportDiagnosticSeverity::BuildBlocking;
    }

    return Reports::BuildReportDiagnosticSeverity::Info;
}

Reports::BuildReportDiagnostic ToReportDiagnostic(const ForgeDiagnostic& diagnostic)
{
    Reports::BuildReportDiagnostic reportDiagnostic;
    reportDiagnostic.severity = ToReportSeverity(diagnostic.severity);
    reportDiagnostic.code = diagnostic.code;
    reportDiagnostic.title = diagnostic.title;
    reportDiagnostic.message = diagnostic.message;
    reportDiagnostic.stepId = diagnostic.stepId;
    reportDiagnostic.reference = diagnostic.reference;
    reportDiagnostic.metadata = diagnostic.metadata;
    return reportDiagnostic;
}

} // namespace

void DiagnosticCollector::Add(ForgeDiagnostic diagnostic)
{
    diagnostics_.push_back(std::move(diagnostic));
}

const std::vector<ForgeDiagnostic>& DiagnosticCollector::Diagnostics() const noexcept
{
    return diagnostics_;
}

bool DiagnosticCollector::Empty() const noexcept
{
    return diagnostics_.empty();
}

std::size_t DiagnosticCollector::Count() const noexcept
{
    return diagnostics_.size();
}

std::size_t DiagnosticCollector::CountBySeverity(
    const ForgeDiagnosticSeverity severity) const
{
    std::size_t count = 0;
    for (const ForgeDiagnostic& diagnostic : diagnostics_)
    {
        if (diagnostic.severity == severity)
        {
            ++count;
        }
    }
    return count;
}

std::size_t DiagnosticCollector::BuildBlockingCount() const
{
    return CountBySeverity(ForgeDiagnosticSeverity::BuildBlocking);
}

void DiagnosticCollector::AppendTo(Reports::BuildReport& report) const
{
    for (const ForgeDiagnostic& diagnostic : diagnostics_)
    {
        report.diagnostics.push_back(ToReportDiagnostic(diagnostic));
    }
}

} // namespace Forge::Diagnostics
