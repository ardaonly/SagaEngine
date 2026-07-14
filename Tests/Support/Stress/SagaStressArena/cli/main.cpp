/// @file main.cpp
/// @brief CLI entry point for bounded SagaStressArena diagnostics scenarios.

#include "SagaStressArena/ScenarioRunner.hpp"

#include <charconv>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace
{

constexpr int kExitSuccess = 0;
constexpr int kExitUsage = 2;

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

[[nodiscard]] bool ParseUint32(std::string_view text, std::uint32_t& outValue)
{
    std::uint32_t value = 0;
    const auto* begin = text.data();
    const auto* end = text.data() + text.size();
    const auto result = std::from_chars(begin, end, value);
    if (result.ec != std::errc{} || result.ptr != end)
    {
        return false;
    }
    outValue = value;
    return true;
}

void PrintUsage(std::ostream& os)
{
    os << "SagaStressArena - bounded local diagnostics stress harness\n\n"
       << "Usage:\n"
       << "  SagaStressArena --help\n"
       << "  SagaStressArena [--scenario direct_zone_diagnostics_smoke]\n"
       << "      [--tier smoke|mini_soak|heavy] [--actors <count>]\n"
       << "      [--ticks <count>] [--max-duration-sec <seconds>]\n"
       << "      [--report-dir <path>] [--fail-fast] [--json]\n\n"
       << "Defaults are smoke tier, 5 actors, 50 ticks, and a 10 second cap.\n"
       << "The heavy tier is documented manual-only in v1 and does not execute.\n";
}

void PrintTextResult(const SagaStressArena::ScenarioRunResult& result)
{
    std::cout << "status: " << SagaStressArena::ToString(result.status) << "\n"
              << "message: " << result.message << "\n"
              << "scenario: " << result.config.scenario << "\n"
              << "tier: " << SagaStressArena::ToString(result.config.tier) << "\n"
              << "operationalSnapshot: "
              << result.reportPaths.operationalSnapshot.generic_string() << "\n"
              << "reliabilityReport: "
              << result.reportPaths.reliabilityReport.generic_string() << "\n"
              << "lifetimeLeaks: "
              << result.reportPaths.lifetimeLeaks.generic_string() << "\n";
    for (const auto& diagnostic : result.diagnostics)
    {
        std::cerr << "[SagaStressArena] " << diagnostic << "\n";
    }
}

void PrintJsonResult(const SagaStressArena::ScenarioRunResult& result)
{
    std::cout << "{"
              << "\"status\":\"" << SagaStressArena::ToString(result.status)
              << "\",\"exitCode\":" << result.exitCode
              << ",\"message\":\"" << result.message
              << "\",\"scenario\":\"" << result.config.scenario
              << "\",\"tier\":\"" << SagaStressArena::ToString(result.config.tier)
              << "\",\"reports\":{"
              << "\"operationalSnapshot\":\""
              << result.reportPaths.operationalSnapshot.generic_string()
              << "\",\"reliabilityReport\":\""
              << result.reportPaths.reliabilityReport.generic_string()
              << "\",\"lifetimeLeaks\":\""
              << result.reportPaths.lifetimeLeaks.generic_string()
              << "\"}}\n";
}

} // namespace

int main(int argc, char* argv[])
{
    std::vector<std::string> args(argv + 1, argv + argc);
    if (TakeBool(args, "help") || TakeBool(args, "h"))
    {
        PrintUsage(std::cout);
        return kExitSuccess;
    }

    SagaStressArena::StressScenarioConfig config =
        SagaStressArena::DefaultStressScenarioConfig();
    config.failFast = TakeBool(args, "fail-fast");
    config.jsonOutput = TakeBool(args, "json");

    std::string scenario;
    std::string tier;
    std::string actors;
    std::string ticks;
    std::string maxDuration;
    std::string reportDir;
    (void)TakeFlag(args, "scenario", scenario);
    (void)TakeFlag(args, "tier", tier);
    (void)TakeFlag(args, "actors", actors);
    (void)TakeFlag(args, "ticks", ticks);
    (void)TakeFlag(args, "max-duration-sec", maxDuration);
    (void)TakeFlag(args, "report-dir", reportDir);

    if (!scenario.empty())
    {
        config.scenario = scenario;
    }
    if (!tier.empty())
    {
        const auto parsedTier = SagaStressArena::ParseStressTier(tier);
        if (!parsedTier.has_value())
        {
            std::cerr << "[SagaStressArena] invalid tier: " << tier << "\n";
            return kExitUsage;
        }
        config.tier = *parsedTier;
    }
    if (!actors.empty())
    {
        std::uint32_t value = 0;
        if (!ParseUint32(actors, value))
        {
            std::cerr << "[SagaStressArena] invalid actors value\n";
            return kExitUsage;
        }
        config.actors = value;
    }
    if (!ticks.empty())
    {
        std::uint32_t value = 0;
        if (!ParseUint32(ticks, value))
        {
            std::cerr << "[SagaStressArena] invalid ticks value\n";
            return kExitUsage;
        }
        config.ticks = value;
    }
    if (!maxDuration.empty())
    {
        std::uint32_t value = 0;
        if (!ParseUint32(maxDuration, value))
        {
            std::cerr << "[SagaStressArena] invalid max-duration-sec value\n";
            return kExitUsage;
        }
        config.maxDurationSec = value;
    }
    if (!reportDir.empty())
    {
        config.reportDirectory = reportDir;
    }

    if (!args.empty())
    {
        std::cerr << "[SagaStressArena] unknown argument: " << args.front()
                  << "\n";
        PrintUsage(std::cerr);
        return kExitUsage;
    }

    const auto result = SagaStressArena::ScenarioRunner::Run(config);
    if (config.jsonOutput)
    {
        PrintJsonResult(result);
    }
    else
    {
        PrintTextResult(result);
    }

    return result.exitCode;
}
