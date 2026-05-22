/// @file main.cpp
/// @brief CLI entry point for Saga-specific pipeline orchestration.

#include "SagaPipeline/EditorCompositionStep.h"

#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

namespace
{

constexpr int kExitSuccess = 0;
constexpr int kExitUsage = 2;
constexpr int kExitRunFail = 3;

[[nodiscard]] bool TakeBool(std::vector<std::string>& args,
                            const std::string& name)
{
    for (auto it = args.begin(); it != args.end(); ++it)
    {
        if (*it == "--" + name)
        {
            args.erase(it);
            return true;
        }
    }
    return false;
}

[[nodiscard]] bool TakeFlag(std::vector<std::string>& args,
                            const std::string& name,
                            std::string& outValue)
{
    const std::string prefix = "--" + name + "=";
    for (auto it = args.begin(); it != args.end(); ++it)
    {
        if (it->rfind(prefix, 0) == 0)
        {
            outValue = it->substr(prefix.size());
            args.erase(it);
            return true;
        }
        if (*it == "--" + name && it + 1 != args.end())
        {
            outValue = *(it + 1);
            args.erase(it, it + 2);
            return true;
        }
    }
    return false;
}

[[nodiscard]] std::string ShellQuote(const std::string& value)
{
    std::string quoted = "'";
    for (char c : value)
    {
        if (c == '\'')
        {
            quoted += "'\\''";
        }
        else
        {
            quoted += c;
        }
    }
    quoted += "'";
    return quoted;
}

void PrintUsage(std::ostream& os)
{
    os << "saga-pipeline - Saga-specific workflow orchestration\n\n"
       << "Usage:\n"
       << "  saga-pipeline --help\n"
       << "  saga-pipeline editor-composition build [--project <path>] [--workspace <path>]\n"
       << "      [--build-root <path>] [--tool <path>] [--composition-id <id>] [--explain]\n";
}

int CmdEditorComposition(std::vector<std::string> args)
{
    if (args.empty())
    {
        std::cerr << "[saga-pipeline/editor-composition] requires a subcommand: build\n";
        return kExitUsage;
    }

    const std::string subcommand = args.front();
    args.erase(args.begin());
    if (subcommand != "build")
    {
        std::cerr << "[saga-pipeline/editor-composition] unknown subcommand: "
                  << subcommand << "\n";
        return kExitUsage;
    }

    SagaPipeline::EditorCompositionBuildRequest request;
    request.explain = TakeBool(args, "explain");

    std::string projectRoot;
    std::string workspaceRoot;
    std::string buildRoot;
    (void)TakeFlag(args, "project", projectRoot);
    (void)TakeFlag(args, "workspace", workspaceRoot);
    (void)TakeFlag(args, "build-root", buildRoot);
    (void)TakeFlag(args, "tool", request.toolExecutable);
    (void)TakeFlag(args, "composition-id", request.compositionId);

    if (!args.empty())
    {
        std::cerr << "[saga-pipeline/editor-composition] unknown build flag: "
                  << args.front() << "\n";
        return kExitUsage;
    }

    if (!projectRoot.empty())
    {
        request.projectRoot = projectRoot;
    }
    if (!workspaceRoot.empty())
    {
        request.workspaceRoot = workspaceRoot;
    }
    if (!buildRoot.empty())
    {
        request.buildRoot = buildRoot;
    }

    SagaPipeline::ProcessExternalToolRunner runner;
    const SagaPipeline::EditorCompositionBuildResult result =
        SagaPipeline::EditorCompositionStep::Run(request, runner);

    if (request.explain)
    {
        std::cout << result.invocation.executable;
        for (const std::string& argument : result.invocation.arguments)
        {
            std::cout << " " << ShellQuote(argument);
        }
        std::cout << "\n"
                  << "workspace: " << result.workspaceRoot.generic_string() << "\n"
                  << "buildRoot: " << result.buildRoot.generic_string() << "\n";
        return kExitSuccess;
    }

    for (const SagaPipeline::PipelineDiagnostic& diagnostic :
         result.pipeline.diagnostics)
    {
        std::cerr << "[saga-pipeline] " << diagnostic.code << ": "
                  << diagnostic.message << "\n";
    }

    if (result.pipeline.exitCode < 0)
    {
        return kExitRunFail;
    }
    return result.pipeline.exitCode;
}

} // namespace

int main(int argc, char* argv[])
{
    std::vector<std::string> args(argv + 1, argv + argc);
    if (args.empty() || args.front() == "--help" || args.front() == "-h")
    {
        PrintUsage(std::cout);
        return kExitSuccess;
    }

    const std::string command = args.front();
    args.erase(args.begin());
    if (command == "editor-composition")
    {
        return CmdEditorComposition(std::move(args));
    }

    std::cerr << "[saga-pipeline] unknown command: " << command << "\n";
    PrintUsage(std::cerr);
    return kExitUsage;
}
