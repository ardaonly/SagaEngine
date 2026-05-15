/// @file ConanAdapter.cpp
/// @brief Conan backend adapter — the only file in Forge that invokes conan.

#include "Forge/ConanAdapter.h"
#include "Forge/BuildScheduler.h"
#include "Forge/ProcessRunner.h"
#include "Forge/ToolEnv.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>

namespace Forge
{

namespace
{

std::string LowerCopy(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c)
    {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

bool ContainsQtToken(const std::string& text)
{
    const std::string lower = LowerCopy(text);
    return lower.find("qt/") != std::string::npos
        || lower.find("\"qt\"") != std::string::npos
        || lower.find("'qt'") != std::string::npos;
}

bool HasHeavyDependency(const BuildModel& model)
{
    for (const auto& dep : model.deps)
    {
        if (LowerCopy(dep.name) == "qt")
            return true;
    }

    std::error_code ec;
    if (!std::filesystem::exists("conanfile.py", ec))
        return false;

    std::ifstream conanfile("conanfile.py");
    if (!conanfile)
        return false;

    const std::string text((std::istreambuf_iterator<char>(conanfile)),
                           std::istreambuf_iterator<char>());
    return ContainsQtToken(text);
}

void LogJobPlan(const JobPlan& plan)
{
    std::cerr << "[forge/scheduler] install"
              << " jobs=" << plan.finalJobs
              << " requested=" << (plan.requestedJobs == 0 ? std::string("auto") : std::to_string(plan.requestedJobs))
              << " detected=" << plan.detectedJobs
              << " safety_limit=" << plan.safetyLimit
              << " cpu=" << plan.hardware.cpuCores
              << " ram_gb=" << BuildScheduler::BytesToGB(plan.hardware.totalRamBytes);
    if (plan.heavyDependency) std::cerr << " heavy=qt";
    if (plan.clamped) std::cerr << " clamped=yes";
    if (plan.forceUnsafeJobs && plan.requestedJobs > 0) std::cerr << " unsafe=yes";
    std::cerr << "\n";

    if (plan.forceUnsafeJobs && plan.requestedJobs > 0)
        std::cerr << "[forge/scheduler] warning: --force-unsafe-jobs bypasses memory and CPU safety clamps.\n";
}

void WriteLock(const std::string&                  mode,
               const ConanAdapter::ProfileOptions& profileOpts,
               const BuildModel&                   model)
{
    std::ofstream lock("forge.lock", std::ios::trunc);
    if (!lock) return;

    lock << "{\n"
         << "  \"forge_lock_version\": 1,\n"
         << "  \"mode\": \"" << mode << "\"";

    if (!profileOpts.host.empty())
    {
        lock << ",\n  \"profile_host\": \"" << profileOpts.host << "\"";
        if (!profileOpts.build.empty())
            lock << ",\n  \"profile_build\": \"" << profileOpts.build << "\"";
    }

    if (mode == "manifest" && !model.deps.empty())
    {
        lock << ",\n  \"deps\": [\n";
        for (std::size_t i = 0; i < model.deps.size(); ++i)
        {
            lock << "    {\"name\": \"" << model.deps[i].name
                 << "\", \"version\": \"" << model.deps[i].version << "\"}";
            if (i + 1 < model.deps.size()) lock << ",";
            lock << "\n";
        }
        lock << "  ]";
    }
    else
    {
        lock << ",\n  \"deps\": []";
    }

    lock << "\n}\n";
}

} // namespace

int ConanAdapter::Install(const BuildModel&               model,
                           const ProfileOptions&           profileOpts,
                           const std::vector<std::string>& extra,
                           bool                            explain)
{
    const std::string& conan = ToolEnv::Active().conan;
    std::vector<std::string> args = {"install", "."};
    std::string mode;

    if (!profileOpts.host.empty())
    {
        mode = "profile";
        args.emplace_back("--profile:host=" + profileOpts.host);
        if (!profileOpts.build.empty())
            args.emplace_back("--profile:build=" + profileOpts.build);
        args.emplace_back("--build=missing");
    }
    else if (!model.deps.empty())
    {
        mode = "manifest";
        for (const auto& dep : model.deps)
            args.emplace_back("--requires=" + dep.name + "/" + dep.version);
        args.emplace_back("--build=missing");
    }
    else if (std::filesystem::exists("conanfile.py"))
    {
        mode = "conanfile";
        args.emplace_back("--build=missing");
    }
    else
    {
        std::cerr << "[forge/conan] no dependencies declared and no conanfile.py found.\n"
                  << "[forge/conan] Use 'forge add <pkg>' to declare dependencies, or add a conanfile.py.\n";
        return 0;
    }

    const JobPlan plan = BuildScheduler::PlanJobs({
        SchedulingPolicy::Safe,
        model.build.jobs,
        model.build.forceUnsafeJobs,
        HasHeavyDependency(model),
    });
    LogJobPlan(plan);
    args.emplace_back("-c");
    args.emplace_back("tools.build:jobs=" + std::to_string(plan.finalJobs));

    for (const auto& e : extra) args.push_back(e);

    std::cerr << "[forge/" << conan << "]";
    for (const auto& a : args) std::cerr << " " << a;
    std::cerr << "\n";

    if (explain) return 0;

    std::string err;
    const int rc = ProcessRunner::Run(conan, args, &err);
    if (rc < 0)
    {
        std::cerr << "[forge/conan] could not launch " << conan << ": " << err
                  << "\n[forge/conan] is `" << conan << "` on PATH? (pipx install conan)\n";
        return 2;
    }
    if (rc == 0) WriteLock(mode, profileOpts, model);
    return rc;
}

} // namespace Forge
