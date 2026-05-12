/// @file main.cpp
/// @brief Forge CLI entry — Cargo-flavoured orchestration frontend for CMake and Conan.

#include "Forge/BuildModel.h"
#include "Forge/BuildScheduler.h"
#include "Forge/CMakeAdapter.h"
#include "Forge/ConanAdapter.h"
#include "Forge/EnvProbe.h"
#include "Forge/Manifest.h"
#include "Forge/ProcessRunner.h"
#include "Forge/ToolEnv.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace
{

// ─── Constants ────────────────────────────────────────────────────────────────

constexpr const char* kForgeVersion = "0.3.0";
constexpr const char* kManifestName = "forge.toml";
constexpr const char* kEnvOverrides = ".forge";

constexpr int kExitSuccess = 0;
constexpr int kExitUsage   = 1;
constexpr int kExitRunFail = 2;
constexpr int kExitStrict  = 3;

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
        "    install [--profile <name>]                install dependencies via Conan\n"
        "            [--profile-build <name>]            optional Conan build profile\n"
        "            [--explain]                         print the conan command, do not run\n"
        "\n"
        "BUILD COMMANDS:\n"
        "    configure  [--source=DIR] [--build=DIR] [--preset=NAME] [--explain]\n"
        "    build      [--build=DIR] [--target=NAME] [--config=Release] [--jobs=N] [--explain]\n"
        "    clean      [--build=DIR] [--explain]\n"
        "    test       [--build=DIR] [--label=LABEL] [--verbose]\n"
        "    install-target [--build=DIR] [--prefix=DIR] [--component=NAME]\n"
        "    presets    [build|test|configure]         list available CMake presets\n"
        "    fmt        [--source=DIR] [--explain]     run clang-format on project sources\n"
        "\n"
        "INFORMATION:\n"
        "    env        [--json]                       show detected tool versions\n"
        "\n"
        "ESCAPE-HATCH:\n"
        "    run <executable> [args ...]               spawn any binary on PATH directly\n"
        "\n"
        "MISC:\n"
        "    --help, -h     print this help\n"
        "    --version      print Forge version\n"
        "    --strict       fail on toolchain mismatches or unknown manifest sections (CI mode)\n"
        "    --explain      print backend commands without executing them\n"
        "    --jobs, -j     explicit parallel job limit (overrides resource-aware scaling)\n"
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

    if (!jobsStr.empty())
    {
        try { model.build.jobs = static_cast<uint32_t>(std::stoul(jobsStr)); }
        catch (...) { std::cerr << "[forge] warning: invalid --jobs value '" << jobsStr << "'\n"; }
    }

    const int rc = Forge::ConanAdapter::Install(model, profileOpts, extra, explain);
    return (rc != 0 && strict) ? kExitStrict : rc;
}

// ─── Build commands ───────────────────────────────────────────────────────────

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

    return 0;
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
        return 0;
    }

    const std::string& fmtExe = Forge::ToolEnv::Active().clangFmt;

    if (explain)
    {
        std::cout << fmtExe << " -i";
        for (const auto& f : files) std::cout << " " << f;
        std::cout << "\n";
        return 0;
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
        return 2;
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

    if (args.empty() || args[0] == "--help" || args[0] == "-h")
    {
        PrintUsage(std::cout);
        return kExitSuccess;
    }
    if (args[0] == "--version") { std::cout << "forge " << kForgeVersion << "\n"; return kExitSuccess; }

    const std::string command = args[0];
    args.erase(args.begin());

    if (command == "new")            return CmdNew(std::move(args));
    if (command == "init")           return CmdInit(std::move(args));
    if (command == "add")            return CmdAdd(std::move(args));
    if (command == "install")        return CmdInstall(std::move(args));
    if (command == "configure")      return CmdConfigure(std::move(args));
    if (command == "build")          return CmdBuild(std::move(args));
    if (command == "clean")          return CmdClean(std::move(args));
    if (command == "test")           return CmdTest(std::move(args));
    if (command == "install-target") return CmdInstallTarget(std::move(args));
    if (command == "presets")        return CmdPresets(std::move(args));
    if (command == "env")            return CmdEnv(std::move(args));
    if (command == "fmt")            return CmdFmt(std::move(args));
    if (command == "run")            return CmdRun(std::move(args));

    // Implicit dispatch is the tools-dispatcher's responsibility. Forge is not a router.
    std::cerr << "[forge] unknown command: '" << command << "'\n";
    PrintUsage(std::cerr);
    return kExitUsage;
}
