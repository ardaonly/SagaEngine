/// @file BuildPlanTests.cpp
/// @brief Deterministic coverage for Forge build plan foundation.

#include "Forge/Pipeline/BuildPlanner.hpp"

#include <cstdlib>
#include <iostream>
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

    std::cerr << "[BuildPlanTests] failed: " << label << "\n";
    return false;
}

bool ExpectIssue(const Forge::BuildPlanValidationResult& result,
                 Forge::BuildPlanIssueKind kind)
{
    for (const Forge::BuildPlanValidationIssue& issue : result.issues)
    {
        if (issue.kind == kind)
        {
            return true;
        }
    }

    std::cerr << "[BuildPlanTests] missing issue kind\n";
    return false;
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
    const Forge::BuildModel model = MakeModel();

    const Forge::BuildPlan buildPlan =
        Forge::BuildPlanner::PlanForCommand(model, Forge::BuildPlanCommand::Build);
    ok &= Expect(buildPlan.steps.size() == 1, "build plan has one step");
    ok &= Expect(buildPlan.steps[0].id == "build-backend", "build step id is stable");
    ok &= Expect(buildPlan.steps[0].kind == Forge::BuildStepKind::BuildBackend,
                 "build step kind is BuildBackend");
    ok &= Expect(buildPlan.steps[0].toolName == "cmake", "build step uses cmake");
    ok &= Expect(buildPlan.steps[0].outputs.size() == 1 &&
                     buildPlan.steps[0].outputs[0] == "target:Game",
                 "build output includes target");

    const Forge::BuildPlan installPlan =
        Forge::BuildPlanner::PlanForCommand(model, Forge::BuildPlanCommand::Install);
    ok &= Expect(installPlan.steps[0].kind == Forge::BuildStepKind::InstallDependencies,
                 "install command plans InstallDependencies");
    ok &= Expect(installPlan.steps[0].toolName == "conan",
                 "install command uses conan");

    const Forge::BuildPlan configurePlan =
        Forge::BuildPlanner::PlanForCommand(model, Forge::BuildPlanCommand::Configure);
    ok &= Expect(configurePlan.steps[0].kind == Forge::BuildStepKind::ConfigureBackend,
                 "configure command plans ConfigureBackend");

    const Forge::BuildPlan testPlan =
        Forge::BuildPlanner::PlanForCommand(model, Forge::BuildPlanCommand::Test);
    ok &= Expect(testPlan.steps[0].kind == Forge::BuildStepKind::RunTests,
                 "test command plans RunTests");

    const Forge::BuildPlan installTargetPlan =
        Forge::BuildPlanner::PlanForCommand(model, Forge::BuildPlanCommand::InstallTarget);
    ok &= Expect(installTargetPlan.steps[0].kind == Forge::BuildStepKind::InstallTarget,
                 "install-target command plans InstallTarget");

    Forge::BuildPlan duplicateOutputs;
    Forge::BuildStep first = MakeStep("first");
    first.outputs.push_back("build/output");
    Forge::BuildStep second = MakeStep("second");
    second.outputs.push_back("build/output");
    duplicateOutputs.steps.push_back(first);
    duplicateOutputs.steps.push_back(second);
    ok &= ExpectIssue(Forge::BuildPlanner::Validate(duplicateOutputs),
                      Forge::BuildPlanIssueKind::DuplicateOutputWriter);

    Forge::BuildPlan missingDependency;
    Forge::BuildStep missing = MakeStep("needs-missing");
    missing.dependencies.push_back("missing");
    missingDependency.steps.push_back(missing);
    ok &= ExpectIssue(Forge::BuildPlanner::Validate(missingDependency),
                      Forge::BuildPlanIssueKind::MissingDependency);

    Forge::BuildPlan cycle;
    Forge::BuildStep a = MakeStep("a");
    Forge::BuildStep b = MakeStep("b");
    a.dependencies.push_back("b");
    b.dependencies.push_back("a");
    cycle.steps.push_back(a);
    cycle.steps.push_back(b);
    ok &= ExpectIssue(Forge::BuildPlanner::Validate(cycle),
                      Forge::BuildPlanIssueKind::DependencyCycle);

    Forge::BuildPlan duplicateIds;
    duplicateIds.steps.push_back(MakeStep("same"));
    duplicateIds.steps.push_back(MakeStep("same"));
    ok &= ExpectIssue(Forge::BuildPlanner::Validate(duplicateIds),
                      Forge::BuildPlanIssueKind::DuplicateStepId);

    ok &= Expect(Forge::BuildPlanner::Validate(buildPlan).IsValid(),
                 "generated build plan validates");

    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
