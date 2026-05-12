/// @file CMakeAdapter.cpp
/// @brief CMake and CTest backend adapter — the only file in Forge that invokes cmake/ctest.

#include "Forge/CMakeAdapter.h"
#include "Forge/BuildScheduler.h"
#include "Forge/ProcessRunner.h"
#include "Forge/ToolEnv.h"

#include <filesystem>
#include <iostream>

namespace Forge
{

namespace
{

void LogCmd(const std::string& exe, const std::vector<std::string>& args)
{
    std::cerr << "[forge/" << exe << "]";
    for (const auto& a : args) std::cerr << " " << a;
    std::cerr << "\n";
}

int Spawn(const std::string& exe, const std::vector<std::string>& args, bool explain)
{
    LogCmd(exe, args);
    if (explain) return 0;

    std::string err;
    const int rc = ProcessRunner::Run(exe, args, &err);
    if (rc < 0)
        std::cerr << "[forge/" << exe << "] could not launch " << exe << ": " << err
                  << "\n[forge/" << exe << "] is `" << exe << "` on PATH?\n";
    return rc < 0 ? 2 : rc;
}

} // namespace

// ─── CMakeAdapter ─────────────────────────────────────────────────────────────

int CMakeAdapter::Configure(const BuildModel& model, const std::vector<std::string>& extra, bool explain)
{
    std::vector<std::string> args;
    if (!model.build.preset.empty())
        args = {"--preset", model.build.preset};
    else
        args = {"-S", model.build.sourceDir, "-B", model.build.buildDir};
    for (const auto& e : extra) args.push_back(e);
    return Spawn(ToolEnv::Active().cmake, args, explain);
}

int CMakeAdapter::Build(const BuildModel& model, const std::vector<std::string>& extra, bool explain)
{
    std::vector<std::string> args = {"--build", model.build.buildDir};
    if (!model.build.target.empty()) { args.emplace_back("--target"); args.emplace_back(model.build.target); }
    if (!model.build.config.empty()) { args.emplace_back("--config"); args.emplace_back(model.build.config); }

    // Apply job scaling (Safe mode by default).
    const uint32_t jobs = BuildScheduler::CalculateJobs(SchedulingPolicy::Safe, model.build.jobs);
    args.emplace_back("-j");
    args.emplace_back(std::to_string(jobs));

    for (const auto& e : extra) args.push_back(e);
    return Spawn(ToolEnv::Active().cmake, args, explain);
}

int CMakeAdapter::Test(const BuildModel& model, const TestOptions& opts,
                        const std::vector<std::string>& extra, bool explain)
{
    const std::string& dir = model.build.buildDir;
    std::vector<std::string> args = {"--test-dir", dir.empty() ? "build" : dir};
    if (!opts.label.empty()) { args.emplace_back("--label-regex"); args.emplace_back(opts.label); }
    if (opts.verbose) args.emplace_back("-V");
    for (const auto& e : extra) args.push_back(e);
    return Spawn(ToolEnv::Active().ctest, args, explain);
}

int CMakeAdapter::InstallTarget(const BuildModel& model, const InstallOptions& opts,
                                  const std::vector<std::string>& extra, bool explain)
{
    const std::string& dir = model.build.buildDir;
    std::vector<std::string> args = {"--install", dir.empty() ? "build" : dir};
    if (!opts.prefix.empty())    { args.emplace_back("--prefix");    args.emplace_back(opts.prefix); }
    if (!opts.component.empty()) { args.emplace_back("--component"); args.emplace_back(opts.component); }
    for (const auto& e : extra) args.push_back(e);
    return Spawn(ToolEnv::Active().cmake, args, explain);
}

int CMakeAdapter::ListPresets(const std::string& type, const std::vector<std::string>& extra)
{
    std::vector<std::string> args;
    if      (type == "build") args = {"--build", "--list-presets"};
    else if (type == "test")  args = {"--test",  "--list-presets"};
    else                      args = {"--list-presets"};
    for (const auto& e : extra) args.push_back(e);
    return Spawn(ToolEnv::Active().cmake, args, /*explain=*/false);
}

int CMakeAdapter::Clean(const BuildModel& model, bool explain)
{
    namespace fs = std::filesystem;
    const std::string& dir = model.build.buildDir;

    if (explain)
    {
        std::cout << "rm -rf " << dir << "\n";
        return 0;
    }

    std::error_code ec;
    if (!fs::exists(dir, ec))
    {
        std::cerr << "[forge/cmake] nothing to clean ('" << dir << "' does not exist)\n";
        return 0;
    }
    const std::uintmax_t removed = fs::remove_all(dir, ec);
    if (ec) { std::cerr << "[forge/cmake] clean failed: " << ec.message() << "\n"; return 2; }
    std::cerr << "[forge/cmake] removed '" << dir << "' (" << removed << " entries)\n";
    return 0;
}

} // namespace Forge
