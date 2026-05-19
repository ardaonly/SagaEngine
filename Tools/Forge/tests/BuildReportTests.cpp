/// @file BuildReportTests.cpp
/// @brief Coverage for Forge build report foundation.

#include "Forge/Pipeline/BuildPlanner.hpp"
#include "Forge/Reports/BuildReportBuilder.hpp"
#include "Forge/Reports/BuildReportWriter.hpp"

#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>
#include <utility>

namespace
{

bool Expect(bool condition, const char* label)
{
    if (condition)
    {
        return true;
    }

    std::cerr << "[BuildReportTests] failed: " << label << "\n";
    return false;
}

bool Contains(const std::string& haystack, const std::string& needle)
{
    return haystack.find(needle) != std::string::npos;
}

Forge::BuildModel MakeModel()
{
    Forge::BuildModel model;
    model.build.sourceDir = ".";
    model.build.buildDir = "build";
    model.build.target = "Game";
    return model;
}

Forge::BuildStep MakeStep(std::string id)
{
    Forge::BuildStep step;
    step.id = std::move(id);
    step.kind = Forge::BuildStepKind::BuildBackend;
    step.toolName = "cmake";
    return step;
}

} // namespace

int main()
{
    bool ok = true;

    const Forge::BuildPlan validPlan =
        Forge::BuildPlanner::PlanForCommand(MakeModel(), Forge::BuildPlanCommand::Build);
    const Forge::BuildPlanValidationResult validValidation =
        Forge::BuildPlanner::Validate(validPlan);
    const Forge::Reports::BuildReport plannedReport =
        Forge::Reports::BuildReportBuilder::CreatePlannedReport(validPlan, validValidation);

    ok &= Expect(plannedReport.status == Forge::Reports::BuildReportStatus::Planned,
                 "valid plan report is Planned");
    ok &= Expect(plannedReport.summary.stepCount == 1, "planned report counts steps");
    ok &= Expect(plannedReport.summary.diagnosticCount == 0,
                 "planned report has no diagnostics");
    ok &= Expect(plannedReport.steps[0].id == "build-backend",
                 "planned report preserves step id");
    ok &= Expect(plannedReport.steps[0].toolName == "cmake",
                 "planned report preserves tool name");

    Forge::BuildPlan invalidPlan;
    Forge::BuildStep first = MakeStep("first");
    first.outputs.push_back("build/output");
    Forge::BuildStep second = MakeStep("second");
    second.outputs.push_back("build/output");
    Forge::BuildStep missing = MakeStep("needs-missing");
    missing.dependencies.push_back("missing");
    invalidPlan.steps.push_back(first);
    invalidPlan.steps.push_back(second);
    invalidPlan.steps.push_back(missing);

    const Forge::BuildPlanValidationResult invalidValidation =
        Forge::BuildPlanner::Validate(invalidPlan);
    const Forge::Reports::BuildReport blockedReport =
        Forge::Reports::BuildReportBuilder::CreatePlannedReport(invalidPlan,
                                                                invalidValidation);

    ok &= Expect(blockedReport.status == Forge::Reports::BuildReportStatus::Blocked,
                 "invalid plan report is Blocked");
    ok &= Expect(blockedReport.summary.diagnosticCount == invalidValidation.issues.size(),
                 "blocked report counts diagnostics");
    ok &= Expect(blockedReport.summary.buildBlockingCount == invalidValidation.issues.size(),
                 "blocked report counts build blockers");

    bool sawDuplicateOutput = false;
    bool sawMissingDependency = false;
    for (const Forge::Reports::BuildReportDiagnostic& diagnostic : blockedReport.diagnostics)
    {
        sawDuplicateOutput |= diagnostic.code == "Forge.BuildPlan.DuplicateOutputWriter";
        sawMissingDependency |= diagnostic.code == "Forge.BuildPlan.MissingDependency";
    }
    ok &= Expect(sawDuplicateOutput, "duplicate output becomes diagnostic");
    ok &= Expect(sawMissingDependency, "missing dependency becomes diagnostic");

    Forge::BuildPlan cyclePlan;
    Forge::BuildStep a = MakeStep("a");
    Forge::BuildStep b = MakeStep("b");
    a.dependencies.push_back("b");
    b.dependencies.push_back("a");
    cyclePlan.steps.push_back(a);
    cyclePlan.steps.push_back(b);
    const Forge::Reports::BuildReport cycleReport =
        Forge::Reports::BuildReportBuilder::CreatePlannedReport(
            cyclePlan,
            Forge::BuildPlanner::Validate(cyclePlan));
    bool sawCycle = false;
    for (const Forge::Reports::BuildReportDiagnostic& diagnostic : cycleReport.diagnostics)
    {
        sawCycle |= diagnostic.code == "Forge.BuildPlan.DependencyCycle";
    }
    ok &= Expect(sawCycle, "dependency cycle becomes diagnostic");

    std::ostringstream json;
    Forge::Reports::BuildReportWriter::WriteJson(blockedReport, json);
    const std::string output = json.str();
    ok &= Expect(Contains(output, "\"status\": \"Blocked\""), "json writes status");
    ok &= Expect(Contains(output, "\"stepCount\": 3"), "json writes summary");
    ok &= Expect(Contains(output, "\"code\": \"Forge.BuildPlan.MissingDependency\""),
                 "json writes diagnostic code");
    ok &= Expect(Contains(output, "\"forge.report.version\": \"1\""),
                 "json writes deterministic metadata");

    Forge::Reports::BuildReport escapingReport;
    escapingReport.status = Forge::Reports::BuildReportStatus::Planned;
    Forge::Reports::BuildReportStep escapingStep;
    escapingStep.id = "quote\"and\nnewline";
    escapingStep.toolName = "tool\\path";
    escapingReport.steps.push_back(escapingStep);
    escapingReport.summary.stepCount = escapingReport.steps.size();
    std::ostringstream escapedJson;
    Forge::Reports::BuildReportWriter::WriteJson(escapingReport, escapedJson);
    ok &= Expect(Contains(escapedJson.str(), "quote\\\"and\\nnewline"),
                 "json escapes quotes and newlines");
    ok &= Expect(Contains(escapedJson.str(), "tool\\\\path"),
                 "json escapes backslashes");

    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
