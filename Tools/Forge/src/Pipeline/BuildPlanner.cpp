/// @file BuildPlanner.cpp
/// @brief BuildPlan lowering and validation.

#include "Forge/Pipeline/BuildPlanner.hpp"

#include <map>
#include <utility>

namespace Forge
{

namespace
{

BuildStep MakeInstallStep()
{
    BuildStep step;
    step.id = "install-dependencies";
    step.kind = BuildStepKind::InstallDependencies;
    step.inputs.push_back("forge.toml");
    step.outputs.push_back("conan-dependencies");
    step.toolName = "conan";
    return step;
}

BuildStep MakeConfigureStep(const BuildModel& model)
{
    BuildStep step;
    step.id = "configure-backend";
    step.kind = BuildStepKind::ConfigureBackend;
    step.inputs.push_back(model.build.preset.empty()
                              ? model.build.sourceDir
                              : "preset:" + model.build.preset);
    step.outputs.push_back(model.build.buildDir);
    step.toolName = "cmake";
    return step;
}

BuildStep MakeBuildStep(const BuildModel& model)
{
    BuildStep step;
    step.id = "build-backend";
    step.kind = BuildStepKind::BuildBackend;
    step.inputs.push_back(model.build.buildDir);
    step.outputs.push_back(model.build.target.empty()
                               ? "cmake-build"
                               : "target:" + model.build.target);
    step.toolName = "cmake";
    return step;
}

BuildStep MakeTestStep(const BuildModel& model)
{
    BuildStep step;
    step.id = "run-tests";
    step.kind = BuildStepKind::RunTests;
    step.inputs.push_back(model.build.buildDir);
    step.outputs.push_back("ctest-results");
    step.toolName = "ctest";
    return step;
}

BuildStep MakeInstallTargetStep(const BuildModel& model)
{
    BuildStep step;
    step.id = "install-target";
    step.kind = BuildStepKind::InstallTarget;
    step.inputs.push_back(model.build.buildDir);
    step.outputs.push_back("install-tree");
    step.toolName = "cmake";
    return step;
}

void AddIssue(BuildPlanValidationResult& result,
              BuildPlanIssueKind kind,
              std::string stepId,
              std::string reference)
{
    result.issues.push_back({kind, std::move(stepId), std::move(reference)});
}

} // namespace

BuildPlan BuildPlanner::PlanForCommand(const BuildModel& model,
                                       const BuildPlanCommand command)
{
    BuildPlan plan;
    switch (command)
    {
        case BuildPlanCommand::Install:
            plan.steps.push_back(MakeInstallStep());
            break;
        case BuildPlanCommand::Configure:
            plan.steps.push_back(MakeConfigureStep(model));
            break;
        case BuildPlanCommand::Build:
            plan.steps.push_back(MakeBuildStep(model));
            break;
        case BuildPlanCommand::Test:
            plan.steps.push_back(MakeTestStep(model));
            break;
        case BuildPlanCommand::InstallTarget:
            plan.steps.push_back(MakeInstallTargetStep(model));
            break;
    }
    return plan;
}

BuildPlanValidationResult BuildPlanner::Validate(const BuildPlan& plan)
{
    BuildPlanValidationResult result;
    std::map<std::string, const BuildStep*> stepsById;
    std::map<std::string, std::string> outputWriters;

    for (const BuildStep& step : plan.steps)
    {
        if (stepsById.find(step.id) != stepsById.end())
        {
            AddIssue(result, BuildPlanIssueKind::DuplicateStepId, step.id, step.id);
        }
        else
        {
            stepsById.emplace(step.id, &step);
        }

        for (const std::string& output : step.outputs)
        {
            if (output.empty())
            {
                continue;
            }

            const auto [it, inserted] = outputWriters.emplace(output, step.id);
            if (!inserted)
            {
                AddIssue(result,
                         BuildPlanIssueKind::DuplicateOutputWriter,
                         step.id,
                         it->first);
            }
        }
    }

    for (const BuildStep& step : plan.steps)
    {
        for (const std::string& dependency : step.dependencies)
        {
            if (stepsById.find(dependency) == stepsById.end())
            {
                AddIssue(result,
                         BuildPlanIssueKind::MissingDependency,
                         step.id,
                         dependency);
            }
        }
    }

    std::map<std::string, int> visitState;
    const auto hasCycle = [&](const auto& self, const BuildStep& step) -> bool
    {
        visitState[step.id] = 1;
        for (const std::string& dependency : step.dependencies)
        {
            const auto depIt = stepsById.find(dependency);
            if (depIt == stepsById.end())
            {
                continue;
            }

            const int state = visitState[dependency];
            if (state == 1)
            {
                AddIssue(result,
                         BuildPlanIssueKind::DependencyCycle,
                         step.id,
                         dependency);
                return true;
            }
            if (state == 0 && self(self, *depIt->second))
            {
                return true;
            }
        }
        visitState[step.id] = 2;
        return false;
    };

    for (const BuildStep& step : plan.steps)
    {
        if (visitState[step.id] == 0)
        {
            static_cast<void>(hasCycle(hasCycle, step));
        }
    }

    return result;
}

} // namespace Forge
