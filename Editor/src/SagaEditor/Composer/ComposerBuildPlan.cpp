/// @file ComposerBuildPlan.cpp
/// @brief External SagaPipeline build command planner implementation.

#include "SagaEditor/Composer/ComposerBuildPlan.h"

#include <filesystem>
#include <sstream>

namespace SagaEditor
{
namespace
{

[[nodiscard]] std::filesystem::path ResolveSiblingOrCommand(
    const std::filesystem::path& composerExecutablePath,
    const std::string& name)
{
    if (!composerExecutablePath.empty())
    {
        const std::filesystem::path candidate =
            composerExecutablePath.parent_path() / name;
        if (std::filesystem::exists(candidate))
        {
            return candidate;
        }
    }
    return name;
}

[[nodiscard]] std::string Quote(const std::string& text)
{
    if (text.find_first_of(" \t\"'") == std::string::npos)
    {
        return text;
    }
    std::string quoted = "'";
    for (char c : text)
    {
        if (c == '\'')
        {
            quoted += "'\\''";
        }
        else
        {
            quoted.push_back(c);
        }
    }
    quoted.push_back('\'');
    return quoted;
}

[[nodiscard]] std::string DisplayCommand(
    const std::filesystem::path& executable,
    const std::vector<std::string>& arguments)
{
    std::ostringstream stream;
    stream << Quote(executable.string());
    for (const std::string& argument : arguments)
    {
        stream << ' ' << Quote(argument);
    }
    return stream.str();
}

} // namespace

ComposerBuildPlan MakeComposerBuildPlan(
    const ComposerWorkspacePaths& paths,
    const std::filesystem::path& composerExecutablePath)
{
    ComposerBuildPlan plan;
    plan.executable =
        ResolveSiblingOrCommand(composerExecutablePath, "saga-pipeline");
    plan.compilerPath = ResolveSiblingOrCommand(
        composerExecutablePath,
        "saga-editor-composition-compiler");
    plan.arguments = {
        "editor-composition",
        "build",
        "--project",
        paths.root.string(),
        "--tool",
        plan.compilerPath.string(),
    };
    plan.displayCommand = DisplayCommand(plan.executable, plan.arguments);
    return plan;
}

} // namespace SagaEditor
