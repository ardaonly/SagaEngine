/// @file ConanAdapter.cpp
/// @brief Conan backend adapter — the only file in Forge that invokes conan.

#include "Forge/ConanAdapter.h"
#include "Forge/BuildScheduler.h"
#include "Forge/ProcessRunner.h"
#include "Forge/ToolEnv.h"

#include <filesystem>
#include <fstream>
#include <iostream>

namespace Forge
{

namespace
{

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

    // Apply job scaling (Safe mode by default).
    const uint32_t jobs = BuildScheduler::CalculateJobs(SchedulingPolicy::Safe, model.build.jobs);
    args.emplace_back("-c");
    args.emplace_back("tools.system.build:jobs=" + std::to_string(jobs));

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
