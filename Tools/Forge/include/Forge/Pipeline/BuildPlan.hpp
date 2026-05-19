/// @file BuildPlan.hpp
/// @brief Backend-neutral Forge build plan contracts.

#pragma once

#include <string>
#include <vector>

namespace Forge
{

/// Supported high-level Forge command families that can be lowered to a plan.
enum class BuildPlanCommand
{
    Install,
    Configure,
    Build,
    Test,
    InstallTarget,
};

/// Executable step families in the early Forge pipeline model.
enum class BuildStepKind
{
    InstallDependencies,
    ConfigureBackend,
    BuildBackend,
    RunTests,
    InstallTarget,
};

/// Failure policy for a build step.
enum class BuildStepFailurePolicy
{
    StopOnFailure,
    ContinueOnFailure,
};

/// One planned build pipeline step. It is metadata only; adapters still execute commands.
struct BuildStep
{
    std::string id;
    BuildStepKind kind = BuildStepKind::BuildBackend;
    std::vector<std::string> inputs;
    std::vector<std::string> outputs;
    std::vector<std::string> dependencies;
    std::string toolName;
    BuildStepFailurePolicy failurePolicy = BuildStepFailurePolicy::StopOnFailure;
};

/// Ordered build plan produced from a BuildModel.
struct BuildPlan
{
    std::vector<BuildStep> steps;
};

/// Validation issue kind for a generated or hand-built plan.
enum class BuildPlanIssueKind
{
    DuplicateStepId,
    MissingDependency,
    DependencyCycle,
    DuplicateOutputWriter,
};

/// One validation issue with stable step/reference fields for tests and reports.
struct BuildPlanValidationIssue
{
    BuildPlanIssueKind kind = BuildPlanIssueKind::MissingDependency;
    std::string stepId;
    std::string reference;
};

/// Validation result for a build plan.
struct BuildPlanValidationResult
{
    std::vector<BuildPlanValidationIssue> issues;

    [[nodiscard]] bool IsValid() const noexcept
    {
        return issues.empty();
    }
};

} // namespace Forge
