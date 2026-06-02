/// @file main.cpp
/// @brief CLI entry point for bounded SagaChaosLab chaos profile runs.

#include "SagaChaosLab/ChaosProfile.hpp"
#include "SagaChaosLab/ChaosRunner.hpp"

#include <iostream>
#include <string>
#include <vector>

namespace
{

constexpr int kExitSuccess = 0;
constexpr int kExitUsage = 2;
constexpr int kExitInvalidProfile = 4;

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

void PrintUsage(std::ostream& os)
{
    os << "SagaChaosLab - bounded local chaos profile runner\n\n"
       << "Usage:\n"
       << "  SagaChaosLab --help\n"
       << "  SagaChaosLab [--profile <chaos_profile.json>] [--allow-manual]\n\n"
       << "Without --profile, SagaChaosLab runs the built-in smoke profile.\n"
       << "Manual-only and heavy profiles require --allow-manual.\n";
}

void PrintResult(const SagaChaosLab::ChaosRunResult& result)
{
    std::cout << "status: " << SagaChaosLab::ToString(result.status) << "\n"
              << "message: " << result.message << "\n"
              << "profileName: " << result.profile.profileName << "\n"
              << "scenario: " << result.profile.scenario << "\n"
              << "tier: " << SagaStressArena::ToString(result.profile.tier)
              << "\n"
              << "chaosReport: "
              << result.chaosReportPath.generic_string() << "\n";

    for (const auto& diagnostic : result.diagnostics)
    {
        std::cerr << "[SagaChaosLab] " << diagnostic << "\n";
    }
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

    SagaChaosLab::ChaosRunnerOptions options;
    options.allowManual = TakeBool(args, "allow-manual");

    std::string profilePath;
    (void)TakeFlag(args, "profile", profilePath);

    if (!args.empty())
    {
        std::cerr << "[SagaChaosLab] unknown argument: " << args.front()
                  << "\n";
        PrintUsage(std::cerr);
        return kExitUsage;
    }

    SagaChaosLab::ChaosProfile profile =
        SagaChaosLab::DefaultSmokeChaosProfile();
    if (!profilePath.empty())
    {
        const auto loaded = SagaChaosLab::LoadChaosProfile(profilePath);
        if (!loaded.Succeeded())
        {
            for (const auto& diagnostic : loaded.diagnostics)
            {
                std::cerr << "[SagaChaosLab] " << diagnostic << "\n";
            }
            return kExitInvalidProfile;
        }
        profile = loaded.profile;
    }

    const auto result = SagaChaosLab::ChaosRunner::Run(profile, options);
    PrintResult(result);
    return result.exitCode;
}
