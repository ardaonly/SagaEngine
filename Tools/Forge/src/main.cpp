/// @file main.cpp
/// @brief Forge CLI entry — Cargo-flavoured orchestration frontend for CMake and Conan.

#include "Forge/BuildModel.h"
#include "Forge/BuildScheduler.h"
#include "Forge/CMakeAdapter.h"
#include "Forge/ConanAdapter.h"
#include "Forge/EnvProbe.h"
#include "Forge/ExitCode.h"
#include "Forge/Manifest.h"
#include "Forge/Pipeline/BuildPlanner.hpp"
#include "Forge/ProcessRunner.h"
#include "Forge/Reports/BuildReportBuilder.hpp"
#include "Forge/Reports/BuildReportWriter.hpp"
#include "Forge/ToolEnv.h"

#include <cstdlib>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <set>
#include <string>
#include <vector>

namespace
{

// ─── Constants ────────────────────────────────────────────────────────────────

constexpr const char* kForgeVersion = "0.3.0";
constexpr const char* kManifestName = "forge.toml";
constexpr const char* kEnvOverrides = ".forge";

constexpr int kExitSuccess = Forge::ToInt(Forge::ExitCode::Success);
constexpr int kExitUsage = Forge::ToInt(Forge::ExitCode::UsageError);
constexpr int kExitRunFail = Forge::ToInt(Forge::ExitCode::ExecutionFailure);
constexpr int kExitStrict = Forge::ToInt(Forge::ExitCode::StrictFailure);

// ─── Argv helpers ─────────────────────────────────────────────────────────────

bool TakeBool(std::vector<std::string>& args, const std::string& name)
{
    for (auto it = args.begin(); it != args.end(); ++it)
    {
        if (*it == "--" + name) { args.erase(it); return true; }
    }
    return false;
}

bool TakeFlag(std::vector<std::string>& args, const std::string& name, std::string& outValue)
{
    const std::string prefix = "--" + name + "=";
    const std::string shortFlag = name.size() == 1 ? "-" + name : std::string();
    const std::string shortPrefix = shortFlag.empty() ? std::string() : shortFlag + "=";
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
        if (!shortPrefix.empty() && it->rfind(shortPrefix, 0) == 0)
        {
            outValue = it->substr(shortPrefix.size());
            args.erase(it);
            return true;
        }
        if (!shortFlag.empty() && *it == shortFlag && it + 1 != args.end())
        {
            outValue = *(it + 1);
            args.erase(it, it + 2);
            return true;
        }
    }
    return false;
}

std::vector<std::string> TakeTrailing(std::vector<std::string>& args)
{
    for (auto it = args.begin(); it != args.end(); ++it)
    {
        if (*it == "--")
        {
            std::vector<std::string> tail(it + 1, args.end());
            args.erase(it, args.end());
            return tail;
        }
    }
    return {};
}

// ─── Nix helpers ──────────────────────────────────────────────────────────────

std::string ShellQuote(const std::string& value)
{
    std::string quoted = "'";
    for (char c : value)
    {
        if (c == '\'')
            quoted += "'\\''";
        else
            quoted += c;
    }
    quoted += "'";
    return quoted;
}

bool IsNixOSHost()
{
#if defined(__linux__)
    std::error_code ec;
    return std::filesystem::exists("/etc/NIXOS", ec);
#else
    return false;
#endif
}

bool IsInsideNixShell()
{
    return std::getenv("IN_NIX_SHELL") != nullptr;
}

bool IsNixReentry()
{
    const char* value = std::getenv("FORGE_NIX_REENTRY");
    return value != nullptr && std::string(value) == "1";
}

bool CommandNeedsProjectToolchain(const std::string& command)
{
    static const std::set<std::string> kCommands = {
        "install",
        "configure",
        "build",
        "test",
        "install-target",
        "presets",
        "env",
        "fmt",
    };
    return kCommands.find(command) != kCommands.end();
}

int CheckNixEnvironment(const std::vector<std::string>& args)
{
    if (args.empty() || !CommandNeedsProjectToolchain(args.front()))
        return -1;
    if (!IsNixOSHost() || IsInsideNixShell() || IsNixReentry())
        return -1;

    std::error_code ec;
    if (!std::filesystem::exists("shell.nix", ec))
        return -1;

    std::cerr << "[forge/nix] NixOS host detected outside nix-shell.\n"
              << "[forge/nix] Run `nix-shell` first, or use `forge nix "
              << args.front() << "` to enter shell.nix explicitly.\n";
    return kExitUsage;
}

// ─── Manifest helpers ─────────────────────────────────────────────────────────

/// Load forge.toml from CWD if present; return an empty manifest otherwise.
Forge::Manifest LoadOrEmpty()
{
    std::error_code ec;
    if (!std::filesystem::exists(kManifestName, ec)) return Forge::Manifest::Default();
    std::string err;
    if (auto m = Forge::Manifest::LoadFromFile(kManifestName, &err)) return *m;
    std::cerr << "[forge] manifest warning: " << err << " (continuing with empty manifest)\n";
    return Forge::Manifest::Default();
}

// ─── Toolchain helpers ────────────────────────────────────────────────────────

/// Log toolchain pins and, in strict mode, verify them via EnvProbe.
/// Returns false only when strict is true and a version mismatch is detected.
bool CheckAndLogToolchain(const Forge::BuildModel& model, bool strict)
{
    const bool hasPins = !model.toolchain.cmake.empty()
                      || !model.toolchain.conan.empty()
                      || !model.toolchain.compiler.empty();

    if (!hasPins)
    {
        if (strict)
            std::cerr << "[forge] --strict: no [toolchain] pins in forge.toml (nothing to verify)\n";
        return true;
    }

    if (strict)
    {
        const auto report = Forge::EnvProbe::Detect(Forge::ToolEnv::Active());
        return Forge::EnvProbe::CheckStrict(report, model);
    }

    std::cerr << "[forge] toolchain pins:";
    if (!model.toolchain.cmake.empty())    std::cerr << "  cmake="    << model.toolchain.cmake;
    if (!model.toolchain.conan.empty())    std::cerr << "  conan="    << model.toolchain.conan;
    if (!model.toolchain.compiler.empty()) std::cerr << "  compiler=" << model.toolchain.compiler;
    std::cerr << "\n";
    return true;
}

/// In strict mode, reject manifests with unknown top-level sections.
/// Returns kExitStrict on violation, kExitSuccess when clean.
int ValidateManifest(const Forge::Manifest& m, bool strict)
{
    if (!strict) return kExitSuccess;
    const auto unknown = m.UnknownSections();
    for (const auto& s : unknown)
        std::cerr << "[forge] --strict: unknown section [" << s << "] in forge.toml\n";
    return unknown.empty() ? kExitSuccess : kExitStrict;
}

// ─── Help text ────────────────────────────────────────────────────────────────

void PrintUsage(std::ostream& os)
{
    os <<
        "Forge " << kForgeVersion << " — Cargo-flavoured build frontend for C++ (CMake + Conan)\n"
        "\n"
        "USAGE:\n"
        "    forge <command> [options] [-- <extra args passed to the backend>]\n"
        "\n"
        "PROJECT COMMANDS:\n"
        "    new <dir>                                 scaffold a new project\n"
        "    init                                      write forge.toml in the current directory\n"
        "    add <pkg>[@<ver>]                         append a dependency to forge.toml\n"
        "\n"
        "DEPENDENCY COMMANDS:\n"
        "    install [--profile <name>] [--jobs=N]     install dependencies via Conan\n"
        "            [--profile-build <name>]            optional Conan build profile\n"
        "            [--explain]                         print the conan command, do not run\n"
        "\n"
        "BUILD COMMANDS:\n"
        "    configure  [--source=DIR] [--build=DIR] [--preset=NAME] [--explain]\n"
        "    build      [--build=DIR] [--target=NAME] [--config=Release] [--jobs=N] [--explain]\n"
        "    clean      [--build=DIR] [--explain]\n"
        "    test       [--build=DIR] [--label=LABEL] [--verbose]\n"
        "    install-target [--build=DIR] [--prefix=DIR] [--component=NAME]\n"
        "    plan       <command> [--json] [--write-report]\n"
        "                                                 preview Forge build plan report\n"
        "    presets    [build|test|configure]         list available CMake presets\n"
        "    fmt        [--source=DIR] [--explain]     run clang-format on project sources\n"
        "\n"
        "INFORMATION:\n"
        "    env        [--json]                       show detected tool versions\n"
        "    report     [--json] [--build=DIR] [--path=FILE]\n"
        "                                                 inspect latest Forge report\n"
        "\n"
        "ESCAPE-HATCH:\n"
        "    nix <forge-command> [args ...]            run a Forge command through repository shell.nix\n"
        "    run <executable> [args ...]               spawn any binary on PATH directly\n"
        "\n"
        "MISC:\n"
        "    --help, -h     print this help\n"
        "    --version      print Forge version\n"
        "    --strict       fail on toolchain mismatches or unknown manifest sections (CI mode)\n"
        "    --explain      print backend commands without executing them\n"
        "    --jobs, -j             requested parallel job count (safety clamps still apply)\n"
        "    --force-unsafe-jobs    let explicit --jobs bypass memory and CPU safety clamps\n"
        "\n"
        "Forge exposes three control levels:\n"
        "    high    :  forge add sfml@2.6  /  forge install  /  forge build\n"
        "    mid     :  forge build --config Debug --target MyApp\n"
        "    low     :  forge run cmake --build build --target MyApp\n"
        "               forge run conan install . --build=missing\n";
}

// ─── Project scaffolding ──────────────────────────────────────────────────────

int CmdNew(std::vector<std::string> args)
{
    if (args.empty()) { std::cerr << "[forge] 'new' requires a directory name.\n"; return kExitUsage; }
    const std::string dir = args.front();

    namespace fs = std::filesystem;
    std::error_code ec;
    if (fs::exists(dir, ec)) { std::cerr << "[forge] '" << dir << "' already exists.\n"; return kExitUsage; }

    fs::create_directories(fs::path(dir) / "src", ec);
    if (ec) { std::cerr << "[forge] mkdir failed: " << ec.message() << "\n"; return kExitRunFail; }

    Forge::Manifest m;
    m.Set("project", "name",    dir);
    m.Set("build",   "backend", "cmake");
    m.Set("build",   "source",  ".");
    m.Set("build",   "build",   "build");

    std::string err;
    if (!m.SaveToFile((fs::path(dir) / kManifestName).string(), &err))
    {
        std::cerr << "[forge] " << err << "\n";
        return kExitRunFail;
    }

    std::ofstream((fs::path(dir) / "src" / "main.cpp").string()) <<
        "#include <cstdio>\nint main() { std::puts(\"hello, forge\"); }\n";

    std::ofstream((fs::path(dir) / "CMakeLists.txt").string()) <<
        "cmake_minimum_required(VERSION 3.22)\nproject(" << dir << ")\n"
        "add_executable(" << dir << " src/main.cpp)\n";

    std::cerr << "[forge] scaffolded '" << dir << "/'\n"
              << "[forge] next: cd " << dir << " && forge configure && forge build\n";
    return kExitSuccess;
}

int CmdInit(std::vector<std::string> /*args*/)
{
    namespace fs = std::filesystem;
    if (fs::exists(kManifestName))
    {
        std::cerr << "[forge] forge.toml already exists.\n";
        return kExitUsage;
    }

    Forge::Manifest m;
    m.Set("project", "name",    fs::current_path().filename().string());
    m.Set("build",   "backend", "cmake");
    m.Set("build",   "source",  ".");
    m.Set("build",   "build",   "build");

    std::string err;
    if (!m.SaveToFile(kManifestName, &err)) { std::cerr << "[forge] " << err << "\n"; return kExitRunFail; }
    std::cerr << "[forge] wrote forge.toml\n";
    return kExitSuccess;
}

// ─── Dependency commands ──────────────────────────────────────────────────────

int CmdAdd(std::vector<std::string> args)
{
    std::string version;
    TakeFlag(args, "version", version);
    if (args.empty()) { std::cerr << "[forge] 'add' requires a package name.\n"; return kExitUsage; }

    std::string name = args.front();
    if (const auto at = name.find('@'); at != std::string::npos)
    {
        if (version.empty()) version = name.substr(at + 1);
        name = name.substr(0, at);
    }
    if (version.empty()) version = "latest";

    Forge::Manifest m = LoadOrEmpty();
    m.AddDep(name, version);

    std::string err;
    if (!m.SaveToFile(kManifestName, &err)) { std::cerr << "[forge] " << err << "\n"; return kExitRunFail; }
    std::cerr << "[forge] added: " << name << " = \"" << version << "\"\n";
    return kExitSuccess;
}

int CmdInstall(std::vector<std::string> args)
{
    const bool strict  = TakeBool(args, "strict");
    const bool explain = TakeBool(args, "explain");
    const bool forceUnsafeJobs = TakeBool(args, "force-unsafe-jobs");

    std::string jobsStr;
    TakeFlag(args, "jobs", jobsStr);
    if (jobsStr.empty()) TakeFlag(args, "j", jobsStr);

    Forge::ConanAdapter::ProfileOptions profileOpts;
    TakeFlag(args, "profile",       profileOpts.host);
    TakeFlag(args, "profile-build", profileOpts.build);
    auto extra = TakeTrailing(args);

    if (!args.empty()) { std::cerr << "[forge] unknown install flag: '" << args.front() << "'\n"; return kExitUsage; }

    Forge::Manifest m = LoadOrEmpty();
    if (const int v = ValidateManifest(m, strict); v != kExitSuccess) return v;

    Forge::BuildModel model = Forge::BuildModel::FromManifest(m);
    if (!CheckAndLogToolchain(model, strict)) return kExitStrict;
    model.build.forceUnsafeJobs = forceUnsafeJobs;

    if (!jobsStr.empty())
    {
        try { model.build.jobs = static_cast<uint32_t>(std::stoul(jobsStr)); }
        catch (...) { std::cerr << "[forge] warning: invalid --jobs value '" << jobsStr << "'\n"; }
    }

    const int rc = Forge::ConanAdapter::Install(model, profileOpts, extra, explain);
    return (rc != 0 && strict) ? kExitStrict : rc;
}

// ─── Build commands ───────────────────────────────────────────────────────────

const char* BuildStepKindName(const Forge::BuildStepKind kind)
{
    switch (kind)
    {
        case Forge::BuildStepKind::InstallDependencies:
            return "InstallDependencies";
        case Forge::BuildStepKind::ConfigureBackend:
            return "ConfigureBackend";
        case Forge::BuildStepKind::BuildBackend:
            return "BuildBackend";
        case Forge::BuildStepKind::RunTests:
            return "RunTests";
        case Forge::BuildStepKind::InstallTarget:
            return "InstallTarget";
    }

    return "BuildBackend";
}

const char* BuildReportStatusName(const Forge::Reports::BuildReportStatus status)
{
    switch (status)
    {
        case Forge::Reports::BuildReportStatus::Unknown:
            return "Unknown";
        case Forge::Reports::BuildReportStatus::Planned:
            return "Planned";
        case Forge::Reports::BuildReportStatus::Blocked:
            return "Blocked";
    }

    return "Unknown";
}

bool ParseBuildPlanCommand(const std::string& command,
                           Forge::BuildPlanCommand& outCommand)
{
    if (command == "install")
    {
        outCommand = Forge::BuildPlanCommand::Install;
        return true;
    }
    if (command == "configure")
    {
        outCommand = Forge::BuildPlanCommand::Configure;
        return true;
    }
    if (command == "build")
    {
        outCommand = Forge::BuildPlanCommand::Build;
        return true;
    }
    if (command == "test")
    {
        outCommand = Forge::BuildPlanCommand::Test;
        return true;
    }
    if (command == "install-target")
    {
        outCommand = Forge::BuildPlanCommand::InstallTarget;
        return true;
    }

    return false;
}

void PrintBuildReportSummary(const Forge::Reports::BuildReport& report,
                             std::ostream& os)
{
    os << "forge plan\n";
    os << "  status: " << BuildReportStatusName(report.status) << "\n";
    os << "  steps: " << report.summary.stepCount << "\n";
    os << "  diagnostics: " << report.summary.diagnosticCount << "\n";
    if (const auto it = report.metadata.find("forge.report.path");
        it != report.metadata.end())
    {
        os << "  report: " << it->second << "\n";
    }

    for (const Forge::Reports::BuildReportStep& step : report.steps)
    {
        os << "  - " << step.id
           << " [" << BuildStepKindName(step.kind) << "]";
        if (!step.toolName.empty())
        {
            os << " tool=" << step.toolName;
        }
        os << "\n";
    }

    for (const Forge::Reports::BuildReportDiagnostic& diagnostic : report.diagnostics)
    {
        os << "  ! " << diagnostic.code;
        if (!diagnostic.stepId.empty())
        {
            os << " step=" << diagnostic.stepId;
        }
        if (!diagnostic.reference.empty())
        {
            os << " ref=" << diagnostic.reference;
        }
        os << "\n";
    }
}

std::filesystem::path BuildReportPathForModel(const Forge::BuildModel& model)
{
    return std::filesystem::path(model.build.buildDir) /
           "Reports" /
           "build_report.json";
}

std::filesystem::path BuildReportPathForBuildDir(const std::string& buildDir)
{
    return std::filesystem::path(buildDir) /
           "Reports" /
           "build_report.json";
}

bool WriteBuildReportFile(const Forge::Reports::BuildReport& report,
                          const std::filesystem::path& path,
                          std::string& outError)
{
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    if (ec)
    {
        outError = "could not create report directory '" +
                   path.parent_path().generic_string() + "': " + ec.message();
        return false;
    }

    std::ofstream out(path, std::ios::trunc);
    if (!out)
    {
        outError = "could not open report file '" +
                   path.generic_string() + "' for writing";
        return false;
    }

    Forge::Reports::BuildReportWriter::WriteJson(report, out);
    if (!out)
    {
        outError = "could not write report file '" +
                   path.generic_string() + "'";
        return false;
    }

    return true;
}

bool ReadTextFile(const std::filesystem::path& path,
                  std::string& outText,
                  std::string& outError)
{
    std::ifstream in(path);
    if (!in)
    {
        outError = "report file not found: '" + path.generic_string() + "'";
        return false;
    }

    outText.assign(std::istreambuf_iterator<char>(in),
                   std::istreambuf_iterator<char>());
    if (!in.eof() && in.fail())
    {
        outError = "could not read report file: '" + path.generic_string() + "'";
        return false;
    }

    return true;
}

bool ReportFileSize(const std::filesystem::path& path,
                    std::uintmax_t& outSize,
                    std::string& outError)
{
    std::error_code ec;
    outSize = std::filesystem::file_size(path, ec);
    if (ec)
    {
        outError = "could not stat report file '" +
                   path.generic_string() + "': " + ec.message();
        return false;
    }

    return true;
}

int CmdPlan(std::vector<std::string> args)
{
    const bool asJson = TakeBool(args, "json");
    const bool writeReport = TakeBool(args, "write-report");
    const bool forceUnsafeJobs = TakeBool(args, "force-unsafe-jobs");
    static_cast<void>(TakeBool(args, "explain"));

    if (args.empty())
    {
        std::cerr << "[forge] 'plan' requires a command: install, configure, build, test, or install-target.\n";
        return kExitUsage;
    }

    const std::string plannedCommandName = args.front();
    args.erase(args.begin());

    Forge::BuildPlanCommand plannedCommand = Forge::BuildPlanCommand::Build;
    if (!ParseBuildPlanCommand(plannedCommandName, plannedCommand))
    {
        std::cerr << "[forge] unknown plan command: '" << plannedCommandName << "'\n";
        return kExitUsage;
    }

    auto extra = TakeTrailing(args);
    if (!extra.empty())
    {
        std::cerr << "[forge] plan does not pass backend arguments.\n";
        return kExitUsage;
    }

    std::string jobsStr;
    TakeFlag(args, "jobs", jobsStr);
    if (jobsStr.empty()) TakeFlag(args, "j", jobsStr);

    Forge::Manifest m = LoadOrEmpty();
    Forge::BuildModel model = Forge::BuildModel::FromManifest(m);

    if (plannedCommand == Forge::BuildPlanCommand::Install)
    {
        std::string ignored;
        TakeFlag(args, "profile", ignored);
        TakeFlag(args, "profile-build", ignored);
    }
    else if (plannedCommand == Forge::BuildPlanCommand::Configure)
    {
        TakeFlag(args, "source", model.build.sourceDir);
        TakeFlag(args, "build", model.build.buildDir);
        TakeFlag(args, "preset", model.build.preset);
    }
    else if (plannedCommand == Forge::BuildPlanCommand::Build)
    {
        TakeFlag(args, "build", model.build.buildDir);
        TakeFlag(args, "target", model.build.target);
        TakeFlag(args, "config", model.build.config);
    }
    else if (plannedCommand == Forge::BuildPlanCommand::Test)
    {
        std::string ignored;
        TakeFlag(args, "build", model.build.buildDir);
        TakeFlag(args, "label", ignored);
        static_cast<void>(TakeBool(args, "verbose"));
    }
    else if (plannedCommand == Forge::BuildPlanCommand::InstallTarget)
    {
        std::string ignored;
        TakeFlag(args, "build", model.build.buildDir);
        TakeFlag(args, "prefix", ignored);
        TakeFlag(args, "component", ignored);
    }

    model.build.forceUnsafeJobs = forceUnsafeJobs;
    if (!jobsStr.empty())
    {
        try { model.build.jobs = static_cast<uint32_t>(std::stoul(jobsStr)); }
        catch (...) { std::cerr << "[forge] warning: invalid --jobs value '" << jobsStr << "'\n"; }
    }

    if (!args.empty())
    {
        std::cerr << "[forge] unknown plan flag: '" << args.front() << "'\n";
        return kExitUsage;
    }

    const Forge::BuildPlan plan =
        Forge::BuildPlanner::PlanForCommand(model, plannedCommand);
    const Forge::BuildPlanValidationResult validation =
        Forge::BuildPlanner::Validate(plan);
    Forge::Reports::BuildReport report =
        Forge::Reports::BuildReportBuilder::CreatePlannedReport(plan, validation);

    if (writeReport)
    {
        const std::filesystem::path reportPath = BuildReportPathForModel(model);
        report.metadata["forge.report.path"] = reportPath.generic_string();

        std::string error;
        if (!WriteBuildReportFile(report, reportPath, error))
        {
            std::cerr << "[forge] " << error << "\n";
            return kExitRunFail;
        }
    }

    if (asJson)
    {
        Forge::Reports::BuildReportWriter::WriteJson(report, std::cout);
    }
    else
    {
        PrintBuildReportSummary(report, std::cout);
    }

    return validation.IsValid() ? kExitSuccess : kExitRunFail;
}

int CmdReport(std::vector<std::string> args)
{
    const bool asJson = TakeBool(args, "json");
    std::string buildDir;
    std::string explicitPath;
    TakeFlag(args, "build", buildDir);
    TakeFlag(args, "path", explicitPath);

    if (!args.empty())
    {
        std::cerr << "[forge] unknown report flag: '" << args.front() << "'\n";
        return kExitUsage;
    }

    std::filesystem::path reportPath;
    if (!explicitPath.empty())
    {
        reportPath = explicitPath;
    }
    else
    {
        if (buildDir.empty())
        {
            Forge::Manifest m = LoadOrEmpty();
            Forge::BuildModel model = Forge::BuildModel::FromManifest(m);
            buildDir = model.build.buildDir;
        }
        reportPath = BuildReportPathForBuildDir(buildDir);
    }

    std::string reportText;
    std::string error;
    if (!ReadTextFile(reportPath, reportText, error))
    {
        std::cerr << "[forge] " << error << "\n";
        return kExitRunFail;
    }

    if (asJson)
    {
        std::cout << reportText;
        if (!reportText.empty() && reportText.back() != '\n')
        {
            std::cout << "\n";
        }
        return kExitSuccess;
    }

    std::uintmax_t size = 0;
    if (!ReportFileSize(reportPath, size, error))
    {
        std::cerr << "[forge] " << error << "\n";
        return kExitRunFail;
    }

    std::cout << "forge report\n";
    std::cout << "  path: " << reportPath.generic_string() << "\n";
    std::cout << "  size: " << size << " bytes\n";
    std::cout << "  hint: use --json to print the report\n";
    return kExitSuccess;
}

int CmdConfigure(std::vector<std::string> args)
{
    const bool strict  = TakeBool(args, "strict");
    const bool explain = TakeBool(args, "explain");
    auto extra = TakeTrailing(args);

    Forge::Manifest m = LoadOrEmpty();
    if (const int v = ValidateManifest(m, strict); v != kExitSuccess) return v;

    Forge::BuildModel model = Forge::BuildModel::FromManifest(m);
    if (!CheckAndLogToolchain(model, strict)) return kExitStrict;

    TakeFlag(args, "source", model.build.sourceDir);
    TakeFlag(args, "build",  model.build.buildDir);
    TakeFlag(args, "preset", model.build.preset);

    if (!args.empty()) { std::cerr << "[forge] unknown configure flag: '" << args.front() << "'\n"; return kExitUsage; }

    return Forge::CMakeAdapter::Configure(model, extra, explain);
}

int CmdBuild(std::vector<std::string> args)
{
    const bool strict  = TakeBool(args, "strict");
    const bool explain = TakeBool(args, "explain");
    const bool forceUnsafeJobs = TakeBool(args, "force-unsafe-jobs");
    auto extra = TakeTrailing(args);

    std::string jobsStr;
    TakeFlag(args, "jobs", jobsStr);
    if (jobsStr.empty()) TakeFlag(args, "j", jobsStr);

    Forge::Manifest m = LoadOrEmpty();
    if (const int v = ValidateManifest(m, strict); v != kExitSuccess) return v;

    Forge::BuildModel model = Forge::BuildModel::FromManifest(m);
    if (!CheckAndLogToolchain(model, strict)) return kExitStrict;

    TakeFlag(args, "build",  model.build.buildDir);
    TakeFlag(args, "target", model.build.target);
    TakeFlag(args, "config", model.build.config);
    model.build.forceUnsafeJobs = forceUnsafeJobs;

    if (!jobsStr.empty())
    {
        try { model.build.jobs = static_cast<uint32_t>(std::stoul(jobsStr)); }
        catch (...) { std::cerr << "[forge] warning: invalid --jobs value '" << jobsStr << "'\n"; }
    }

    if (!args.empty()) { std::cerr << "[forge] unknown build flag: '" << args.front() << "'\n"; return kExitUsage; }

    return Forge::CMakeAdapter::Build(model, extra, explain);
}

int CmdClean(std::vector<std::string> args)
{
    const bool explain = TakeBool(args, "explain");

    Forge::Manifest   m     = LoadOrEmpty();
    Forge::BuildModel model = Forge::BuildModel::FromManifest(m);

    TakeFlag(args, "build", model.build.buildDir);
    if (!args.empty()) { std::cerr << "[forge] unknown clean flag: '" << args.front() << "'\n"; return kExitUsage; }

    return Forge::CMakeAdapter::Clean(model, explain);
}

int CmdTest(std::vector<std::string> args)
{
    const bool explain = TakeBool(args, "explain");
    auto extra = TakeTrailing(args);

    Forge::CMakeAdapter::TestOptions opts;
    TakeFlag(args, "label", opts.label);
    opts.verbose = TakeBool(args, "verbose");

    Forge::Manifest   m     = LoadOrEmpty();
    Forge::BuildModel model = Forge::BuildModel::FromManifest(m);
    TakeFlag(args, "build", model.build.buildDir);

    if (!args.empty()) { std::cerr << "[forge] unknown test flag: '" << args.front() << "'\n"; return kExitUsage; }

    return Forge::CMakeAdapter::Test(model, opts, extra, explain);
}

int CmdInstallTarget(std::vector<std::string> args)
{
    const bool explain = TakeBool(args, "explain");
    auto extra = TakeTrailing(args);

    Forge::CMakeAdapter::InstallOptions opts;
    TakeFlag(args, "prefix",    opts.prefix);
    TakeFlag(args, "component", opts.component);

    Forge::Manifest   m     = LoadOrEmpty();
    Forge::BuildModel model = Forge::BuildModel::FromManifest(m);
    TakeFlag(args, "build", model.build.buildDir);

    if (!args.empty()) { std::cerr << "[forge] unknown install-target flag: '" << args.front() << "'\n"; return kExitUsage; }

    return Forge::CMakeAdapter::InstallTarget(model, opts, extra, explain);
}

int CmdPresets(std::vector<std::string> args)
{
    auto extra = TakeTrailing(args);
    const std::string type = args.empty() ? "" : args.front();
    return Forge::CMakeAdapter::ListPresets(type, extra);
}

int CmdEnv(std::vector<std::string> args)
{
    const bool asJson = TakeBool(args, "json");

    Forge::Manifest   m     = LoadOrEmpty();
    Forge::BuildModel model = Forge::BuildModel::FromManifest(m);

    const auto report = Forge::EnvProbe::Detect(Forge::ToolEnv::Active());

    if (asJson)
        Forge::EnvProbe::PrintJson(report, model, std::cout);
    else
        Forge::EnvProbe::PrintTable(report, model, std::cout);

    return kExitSuccess;
}

int CmdFmt(std::vector<std::string> args)
{
    const bool explain = TakeBool(args, "explain");

    std::string sourceDir;
    TakeFlag(args, "source", sourceDir);

    if (sourceDir.empty())
    {
        Forge::Manifest m = LoadOrEmpty();
        sourceDir = m.Get("build", "source", ".");
    }

    namespace fs = std::filesystem;
    std::vector<std::string> files;
    std::error_code ec;
    for (const auto& entry : fs::recursive_directory_iterator(sourceDir, ec))
    {
        if (!entry.is_regular_file()) continue;
        const auto ext = entry.path().extension().string();
        if (ext == ".cpp" || ext == ".h"  || ext == ".hpp" ||
            ext == ".cxx" || ext == ".cc" || ext == ".hxx")
            files.push_back(entry.path().string());
    }

    if (files.empty())
    {
        std::cerr << "[forge/fmt] no source files found under '" << sourceDir << "'\n";
        return kExitSuccess;
    }

    const std::string& fmtExe = Forge::ToolEnv::Active().clangFmt;

    if (explain)
    {
        std::cout << fmtExe << " -i";
        for (const auto& f : files) std::cout << " " << f;
        std::cout << "\n";
        return kExitSuccess;
    }

    std::cerr << "[forge/" << fmtExe << "] formatting " << files.size()
              << " file(s) under '" << sourceDir << "'\n";

    std::vector<std::string> fmtArgs = {"-i"};
    fmtArgs.insert(fmtArgs.end(), files.begin(), files.end());

    std::string err;
    const int rc = Forge::ProcessRunner::Run(fmtExe, fmtArgs, &err);
    if (rc < 0)
    {
        std::cerr << "[forge/fmt] could not launch " << fmtExe << ": " << err
                  << "\n[forge/fmt] is `" << fmtExe << "` on PATH?\n";
        return kExitRunFail;
    }
    return rc;
}

// ─── Nix explicit entrypoint ──────────────────────────────────────────────────

int CmdNix(const std::string& self, std::vector<std::string> args)
{
    if (args.empty())
    {
        std::cerr << "[forge/nix] requires a Forge command, for example: forge nix install --profile linux-gcc\n";
        return kExitUsage;
    }

    std::error_code ec;
    if (!std::filesystem::exists("shell.nix", ec))
    {
        std::cerr << "[forge/nix] shell.nix was not found in the current directory.\n";
        return kExitUsage;
    }

    std::string command = "FORGE_NIX_REENTRY=1 " + ShellQuote(self);
    for (const auto& arg : args)
    {
        command += ' ';
        command += ShellQuote(arg);
    }

    std::cerr << "[forge/nix] nix-shell --run " << command << "\n";

    std::string err;
    const int rc = Forge::ProcessRunner::Run("nix-shell", {"--run", command}, &err);
    if (rc < 0)
    {
        std::cerr << "[forge/nix] could not launch nix-shell: " << err << "\n";
        return kExitRunFail;
    }
    return rc;
}

// ─── Escape-hatch ─────────────────────────────────────────────────────────────

int CmdRun(std::vector<std::string> args)
{
    if (args.empty()) { std::cerr << "[forge] 'run' requires an executable name.\n"; return kExitUsage; }
    const std::string exe = args.front();
    args.erase(args.begin());
    auto trailing = TakeTrailing(args);
    for (auto& t : trailing) args.push_back(std::move(t));

    std::cerr << "[forge] " << exe;
    for (const auto& a : args) std::cerr << " " << a;
    std::cerr << "\n";

    std::string err;
    const int rc = Forge::ProcessRunner::Run(exe, args, &err);
    if (rc < 0) { std::cerr << "[forge] launch failed: " << err << "\n"; return kExitRunFail; }
    return rc;
}

} // namespace

// ─── Entry Point ──────────────────────────────────────────────────────────────

int main(int argc, char** argv)
{
    std::vector<std::string> args;
    args.reserve(static_cast<std::size_t>(argc) > 0 ? argc - 1 : 0);
    for (int i = 1; i < argc; ++i) args.emplace_back(argv[i]);

    if (args.empty() || args[0] == "--help" || args[0] == "-h")
    {
        PrintUsage(std::cout);
        return kExitSuccess;
    }
    if (args[0] == "--version") { std::cout << "forge " << kForgeVersion << "\n"; return kExitSuccess; }

    if (const int nixRc = CheckNixEnvironment(args); nixRc >= 0)
        return nixRc;

    // Load .forge env-overrides from CWD if present.
    if (std::filesystem::exists(kEnvOverrides))
    {
        std::string envErr;
        auto env = Forge::ToolEnv::LoadFromFile(kEnvOverrides, &envErr);
        if (envErr.empty())
            Forge::ToolEnv::SetActive(std::move(env));
        else
            std::cerr << "[forge] warning: .forge parse error: " << envErr << "\n";
    }

    const std::string command = args[0];
    args.erase(args.begin());

    if (command == "nix")            return CmdNix(argv[0], std::move(args));
    if (command == "new")            return CmdNew(std::move(args));
    if (command == "init")           return CmdInit(std::move(args));
    if (command == "add")            return CmdAdd(std::move(args));
    if (command == "install")        return CmdInstall(std::move(args));
    if (command == "configure")      return CmdConfigure(std::move(args));
    if (command == "build")          return CmdBuild(std::move(args));
    if (command == "clean")          return CmdClean(std::move(args));
    if (command == "test")           return CmdTest(std::move(args));
    if (command == "install-target") return CmdInstallTarget(std::move(args));
    if (command == "plan")           return CmdPlan(std::move(args));
    if (command == "presets")        return CmdPresets(std::move(args));
    if (command == "env")            return CmdEnv(std::move(args));
    if (command == "report")         return CmdReport(std::move(args));
    if (command == "fmt")            return CmdFmt(std::move(args));
    if (command == "run")            return CmdRun(std::move(args));

    // Implicit dispatch is the tools-dispatcher's responsibility. Forge is not a router.
    std::cerr << "[forge] unknown command: '" << command << "'\n";
    PrintUsage(std::cerr);
    return kExitUsage;
}
