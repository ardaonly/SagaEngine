/// @file DiagnosticCollectorTests.cpp
/// @brief Coverage for Forge diagnostic collector foundation.

#include "Forge/Diagnostics/DiagnosticCollector.hpp"

#include <cstdlib>
#include <iostream>
#include <string>

namespace
{

bool Expect(bool condition, const char* label)
{
    if (condition)
    {
        return true;
    }

    std::cerr << "[DiagnosticCollectorTests] failed: " << label << "\n";
    return false;
}

Forge::Diagnostics::ForgeDiagnostic MakeDiagnostic(
    Forge::Diagnostics::ForgeDiagnosticSeverity severity,
    std::string code)
{
    Forge::Diagnostics::ForgeDiagnostic diagnostic;
    diagnostic.severity = severity;
    diagnostic.code = std::move(code);
    diagnostic.title = "title";
    diagnostic.message = "message";
    diagnostic.stepId = "step";
    diagnostic.reference = "reference";
    diagnostic.metadata.emplace("key", "value");
    return diagnostic;
}

} // namespace

int main()
{
    bool ok = true;

    Forge::Diagnostics::DiagnosticCollector collector;
    ok &= Expect(collector.Empty(), "default collector is empty");
    ok &= Expect(collector.Count() == 0, "default count is zero");

    collector.Add(MakeDiagnostic(Forge::Diagnostics::ForgeDiagnosticSeverity::Info,
                                 "Forge.Test.Info"));
    collector.Add(MakeDiagnostic(Forge::Diagnostics::ForgeDiagnosticSeverity::Error,
                                 "Forge.Test.Error"));
    collector.Add(MakeDiagnostic(Forge::Diagnostics::ForgeDiagnosticSeverity::BuildBlocking,
                                 "Forge.Test.Blocked"));

    ok &= Expect(!collector.Empty(), "collector is not empty after add");
    ok &= Expect(collector.Count() == 3, "collector counts diagnostics");
    ok &= Expect(collector.CountBySeverity(
                     Forge::Diagnostics::ForgeDiagnosticSeverity::Info) == 1,
                 "collector counts info diagnostics");
    ok &= Expect(collector.CountBySeverity(
                     Forge::Diagnostics::ForgeDiagnosticSeverity::Error) == 1,
                 "collector counts error diagnostics");
    ok &= Expect(collector.BuildBlockingCount() == 1,
                 "collector counts build blockers");

    Forge::Reports::BuildReport report;
    collector.AppendTo(report);
    ok &= Expect(report.diagnostics.size() == 3,
                 "collector appends diagnostics to report");
    ok &= Expect(report.diagnostics[2].severity ==
                     Forge::Reports::BuildReportDiagnosticSeverity::BuildBlocking,
                 "collector maps build blocking severity");
    ok &= Expect(report.diagnostics[2].code == "Forge.Test.Blocked",
                 "collector preserves code");
    ok &= Expect(report.diagnostics[2].stepId == "step",
                 "collector preserves step id");
    ok &= Expect(report.diagnostics[2].reference == "reference",
                 "collector preserves reference");
    ok &= Expect(report.diagnostics[2].metadata.at("key") == "value",
                 "collector preserves metadata");

    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
