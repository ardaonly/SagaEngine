/// @file main.cpp
/// @brief CLI entrypoint for bounded MultiplayerSandbox headless evaluation.

#include "MultiplayerSandboxHeadless/HeadlessEvaluation.hpp"

#include <cstdlib>
#include <iostream>
#include <string>

namespace
{

using MultiplayerSandboxHeadless::HeadlessEvaluationOptions;

struct ParseResult
{
    bool ok{true};
    bool help{false};
    std::string error;
    HeadlessEvaluationOptions options;
};

[[nodiscard]] bool HasValue(int index, int argc) noexcept
{
    return index + 1 < argc;
}

[[nodiscard]] ParseResult ParseArgs(int argc, char* argv[])
{
    ParseResult result;
    for (int i = 1; i < argc; ++i)
    {
        const std::string arg = argv[i] ? argv[i] : "";
        if (arg == "--help" || arg == "--?")
        {
            result.help = true;
            return result;
        }
        if (arg == "--starter-arena-server-smoke")
        {
            result.options.starterArenaServerSmoke = true;
        }
        else if (arg == "--project" && HasValue(i, argc))
        {
            result.options.projectPath = argv[++i];
        }
        else if (arg == "--report-out" && HasValue(i, argc))
        {
            result.options.reportOut = argv[++i];
        }
        else if (arg == "--diagnostics-out" && HasValue(i, argc))
        {
            result.options.diagnosticsOut = argv[++i];
        }
        else if (arg == "--ticks" && HasValue(i, argc))
        {
            result.options.ticks = std::atoi(argv[++i]);
        }
        else if (arg == "--fixed-dt" && HasValue(i, argc))
        {
            result.options.fixedDtSeconds =
                static_cast<float>(std::atof(argv[++i]));
        }
        else
        {
            result.ok = false;
            result.error = "unknown or incomplete argument: " + arg;
            return result;
        }
    }

    if (result.options.projectPath.empty())
    {
        result.ok = false;
        result.error = "--project is required";
    }
    else if (result.options.reportOut.empty())
    {
        result.ok = false;
        result.error = "--report-out is required";
    }
    else if (result.options.diagnosticsOut.empty())
    {
        result.ok = false;
        result.error = "--diagnostics-out is required";
    }
    return result;
}

void PrintUsage()
{
    std::cout
        << "Usage: MultiplayerSandboxHeadless --project <path> "
           "--report-out <path> --diagnostics-out <dir> "
           "[--starter-arena-server-smoke] "
           "[--ticks <n>] [--fixed-dt <seconds>]\n";
}

} // namespace

int main(int argc, char* argv[])
{
    const ParseResult parsed = ParseArgs(argc, argv);
    if (parsed.help)
    {
        PrintUsage();
        return 0;
    }
    if (!parsed.ok)
    {
        std::cerr << parsed.error << '\n';
        PrintUsage();
        return 2;
    }

    const auto report =
        MultiplayerSandboxHeadless::RunHeadlessEvaluation(parsed.options);

    std::string error;
    if (!MultiplayerSandboxHeadless::WriteHeadlessEvaluationReportJson(
            report,
            parsed.options.reportOut,
            error))
    {
        std::cerr << error << '\n';
        return 4;
    }

    return report.status == "Passed" ? 0 : 1;
}
