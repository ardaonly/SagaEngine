/// @file main.cpp
/// @brief Forge CLI entry — Cargo-flavoured wrapper around CMake + Conan.

#include "Forge/Manifest.h"
#include "Forge/ProcessRunner.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace
{

// ─── Constants ────────────────────────────────────────────────────────────────

constexpr const char* kForgeVersion  = "0.2.0";
constexpr const char* kManifestName  = "forge.toml";
constexpr const char* kEnvOverrides  = ".forge"; // env-style toolchain overrides

constexpr int kExitSuccess  = 0;
constexpr int kExitUsage    = 1;
constexpr int kExitRunFail  = 2;
constexpr int kExitStrict   = 3;

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

/// Load `forge.toml` from CWD if present; otherwise return an empty manifest.
/// A missing manifest is normal: most subcommands work without one. Strict
/// mode enforces presence per-subcommand.
Forge::Manifest LoadOrEmpty()
{
    std::error_code ec;
    if (!std::filesystem::exists(kManifestName, ec)) return Forge::Manifest::Default();
    std::string err;
    if (auto m = Forge::Manifest::LoadFromFile(kManifestName, &err)) return *m;
    std::cerr << "[forge] WARNING: " << err << " (continuing with empty manifest)\n";
    return Forge::Manifest::Default();
}

/// Emit a single-line summary of the active toolchain pin, if any. The
/// current implementation does not enforce versions yet — that lands behind
/// `--strict` once the FORGE_ROADMAP toolchain-isolation item ships.
void LogToolchain(const Forge::Manifest& m, bool strict)
{
    const std::string cmake    = m.Get("toolchain", "cmake");
    const std::string conan    = m.Get("toolchain", "conan");
    const std::string compiler = m.Get("toolchain", "compiler");
    if (cmake.empty() && conan.empty() && compiler.empty())
    {
        if (strict)
            std::cerr << "[forge] --strict but no [toolchain] section in forge.toml\n";
        return;
    }
    std::cerr << "[forge] toolchain pins:"
              << (cmake.empty()    ? "" : "  cmake=" + cmake)
              << (conan.empty()    ? "" : "  conan=" + conan)
              << (compiler.empty() ? "" : "  compiler=" + compiler)
              << "\n";
    std::cerr << "[forge]   (enforcement landing in a future Forge release)\n";
}

// ─── Help text ────────────────────────────────────────────────────────────────

void PrintUsage(std::ostream& os)
{
    os <<
        "Forge — Cargo-flavoured build helper for C++ projects (CMake + Conan)\n"
        "\n"
        "USAGE:\n"
        "    forge <command> [options] [-- <extra args>]\n"
        "\n"
        "PROJECT COMMANDS:\n"
        "    new <dir>                         scaffold a new project (forge.toml + dirs)\n"
        "    init                              create forge.toml in the current directory\n"
        "    add <pkg>[@<ver>]                 append a dependency to forge.toml\n"
        "    install                           install dependencies via Conan (reads [deps])\n"
        "\n"
        "BUILD COMMANDS:\n"
        "    configure  [--source=DIR] [--build=DIR] [--preset=NAME]\n"
        "    build      [--build=DIR] [--target=NAME] [--config=Release]\n"
        "    clean      [--build=DIR]\n"
        "\n"
        "ESCAPE-HATCH (low-level):\n"
        "    run <executable> [args ...]       spawn ANY binary on PATH\n"
        "                                      (e.g. `forge run cmake --version`)\n"
        "\n"
        "MISC:\n"
        "    --help, -h                        print this help and exit 0\n"
        "    --version                         print Forge's version and exit 0\n"
        "\n"
        "GLOBAL FLAGS:\n"
        "    --strict                          fail when toolchain pins or lockfiles\n"
        "                                      cannot be honoured (CI / reproducible mode)\n"
        "\n"
        "Forge wraps `cmake` and (optionally) `conan`. It does not hide them — it\n"
        "exposes three layers of control:\n"
        "    1. high-level   :  forge add sfml@2.6  /  forge install  /  forge build\n"
        "    2. intermediate :  forge build --config Debug --target MyGame\n"
        "    3. low-level    :  forge run conan install . --build=missing\n"
        "                       forge run cmake --build build --target MyGame\n";
}

// ─── Project scaffolding ──────────────────────────────────────────────────────

int CmdNew(std::vector<std::string> args)
{
    if (args.empty()) { std::cerr << "[forge] 'new' requires a directory name.\n"; return kExitUsage; }
    const std::string dir = args.front();

    namespace fs = std::filesystem;
    std::error_code ec;
    if (fs::exists(dir, ec))
    {
        std::cerr << "[forge] '" << dir << "' already exists.\n";
        return kExitUsage;
    }

    fs::create_directories(fs::path(dir) / "src", ec);
    if (ec) { std::cerr << "[forge] mkdir failed: " << ec.message() << "\n"; return kExitRunFail; }

    Forge::Manifest m;
    m.Set("project",   "name",    dir);
    m.Set("build",     "backend", "cmake");
    m.Set("build",     "preset",  "release");
    m.Set("build",     "source",  ".");
    m.Set("build",     "build",   "build");
    std::string err;
    if (!m.SaveToFile((fs::path(dir) / kManifestName).string(), &err))
    {
        std::cerr << "[forge] " << err << "\n";
        return kExitRunFail;
    }

    std::ofstream(fs::path(dir) / "src" / "main.cpp") <<
        "// Hello from Forge.\n"
        "#include <cstdio>\nint main() { std::puts(\"hello, forge\"); }\n";

    std::cerr << "[forge] scaffolded project at '" << dir << "/'\n"
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
    m.Set("project",   "name",    fs::current_path().filename().string());
    m.Set("build",     "backend", "cmake");
    m.Set("build",     "preset",  "release");
    m.Set("build",     "source",  ".");
    m.Set("build",     "build",   "build");
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
    if (auto at = name.find('@'); at != std::string::npos)
    {
        if (version.empty()) version = name.substr(at + 1);
        name = name.substr(0, at);
    }
    if (version.empty()) version = "latest";

    Forge::Manifest m = LoadOrEmpty();
    m.AddDep(name, version);

    std::string err;
    if (!m.SaveToFile(kManifestName, &err))
    {
        std::cerr << "[forge] " << err << "\n";
        return kExitRunFail;
    }
    std::cerr << "[forge] added: " << name << " = \"" << version << "\"\n";
    return kExitSuccess;
}

int CmdInstall(std::vector<std::string> args)
{
    const bool strict = TakeBool(args, "strict");
    auto extra = TakeTrailing(args);

    Forge::Manifest m = LoadOrEmpty();
    LogToolchain(m, strict);

    if (m.Deps().empty())
    {
        std::cerr << "[forge] no [deps] in forge.toml — nothing to install.\n";
        return kExitSuccess;
    }

    std::vector<std::string> conanArgs = {"install", "."};
    for (const auto& [name, version] : m.Deps())
        conanArgs.emplace_back("--requires=" + name + "/" + version);
    conanArgs.emplace_back("--build=missing");
    for (auto& e : extra) conanArgs.push_back(std::move(e));

    std::cerr << "[forge] conan";
    for (const auto& a : conanArgs) std::cerr << " " << a;
    std::cerr << "\n";

    std::string err;
    const int rc = Forge::ProcessRunner::Run("conan", conanArgs, &err);
    if (rc < 0)
    {
        std::cerr << "[forge] could not invoke conan: " << err << "\n"
                  << "[forge] is `conan` on PATH? (pipx install conan)\n";
        return strict ? kExitStrict : kExitRunFail;
    }
    return rc;
}

// ─── Build commands ───────────────────────────────────────────────────────────

int CmdConfigure(std::vector<std::string> args)
{
    const bool strict = TakeBool(args, "strict");
    Forge::Manifest m = LoadOrEmpty();
    LogToolchain(m, strict);

    std::string source = m.Get("build", "source", ".");
    std::string build  = m.Get("build", "build",  "build");
    std::string preset = m.Get("build", "preset");
    TakeFlag(args, "source", source);
    TakeFlag(args, "build",  build);
    TakeFlag(args, "preset", preset);
    auto extra = TakeTrailing(args);

    if (!args.empty()) { std::cerr << "[forge] unknown configure flag: '" << args.front() << "'\n"; return kExitUsage; }

    std::vector<std::string> cmakeArgs;
    if (!preset.empty()) cmakeArgs = {"--preset", preset};
    else                 cmakeArgs = {"-S", source, "-B", build};
    for (auto& e : extra) cmakeArgs.push_back(std::move(e));

    std::cerr << "[forge] cmake";
    for (const auto& a : cmakeArgs) std::cerr << " " << a;
    std::cerr << "\n";

    std::string err;
    const int rc = Forge::ProcessRunner::Run("cmake", cmakeArgs, &err);
    if (rc < 0)
    {
        std::cerr << "[forge] could not invoke cmake: " << err << "\n"
                  << "[forge] is `cmake` on PATH?\n";
        return kExitRunFail;
    }
    return rc;
}

int CmdBuild(std::vector<std::string> args)
{
    const bool strict = TakeBool(args, "strict");
    Forge::Manifest m = LoadOrEmpty();
    LogToolchain(m, strict);

    std::string build  = m.Get("build", "build", "build");
    std::string target;
    std::string config;
    TakeFlag(args, "build",  build);
    TakeFlag(args, "target", target);
    TakeFlag(args, "config", config);
    auto extra = TakeTrailing(args);

    if (!args.empty()) { std::cerr << "[forge] unknown build flag: '" << args.front() << "'\n"; return kExitUsage; }

    std::vector<std::string> cmakeArgs = {"--build", build};
    if (!target.empty()) { cmakeArgs.emplace_back("--target"); cmakeArgs.emplace_back(target); }
    if (!config.empty()) { cmakeArgs.emplace_back("--config"); cmakeArgs.emplace_back(config); }
    for (auto& e : extra) cmakeArgs.push_back(std::move(e));

    std::cerr << "[forge] cmake";
    for (const auto& a : cmakeArgs) std::cerr << " " << a;
    std::cerr << "\n";

    std::string err;
    const int rc = Forge::ProcessRunner::Run("cmake", cmakeArgs, &err);
    if (rc < 0) { std::cerr << "[forge] could not invoke cmake: " << err << "\n"; return kExitRunFail; }
    return rc;
}

int CmdClean(std::vector<std::string> args)
{
    std::string build = "build";
    Forge::Manifest m = LoadOrEmpty();
    if (auto fromManifest = m.Get("build", "build"); !fromManifest.empty()) build = fromManifest;
    TakeFlag(args, "build", build);
    if (!args.empty()) { std::cerr << "[forge] unknown clean flag: '" << args.front() << "'\n"; return kExitUsage; }

    namespace fs = std::filesystem;
    std::error_code ec;
    if (!fs::exists(build, ec))
    {
        std::cerr << "[forge] nothing to clean (no '" << build << "' directory)\n";
        return kExitSuccess;
    }
    const std::uintmax_t removed = fs::remove_all(build, ec);
    if (ec) { std::cerr << "[forge] failed: " << ec.message() << "\n"; return kExitRunFail; }
    std::cerr << "[forge] removed '" << build << "' (" << removed << " entries)\n";
    return kExitSuccess;
}

// ─── Escape-hatch ─────────────────────────────────────────────────────────────

int CmdRun(std::vector<std::string> args)
{
    if (args.empty()) { std::cerr << "[forge] 'run' requires an executable name.\n"; return kExitUsage; }
    const std::string exe = args.front();
    args.erase(args.begin());
    auto trailing = TakeTrailing(args); // drop the optional `--` separator
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

    const std::string command = args[0];
    args.erase(args.begin());

    if (command == "new")        return CmdNew(std::move(args));
    if (command == "init")       return CmdInit(std::move(args));
    if (command == "add")        return CmdAdd(std::move(args));
    if (command == "install")    return CmdInstall(std::move(args));
    if (command == "configure")  return CmdConfigure(std::move(args));
    if (command == "build")      return CmdBuild(std::move(args));
    if (command == "clean")      return CmdClean(std::move(args));
    if (command == "run")        return CmdRun(std::move(args));

    // Note for the future: keep this branch a hard error. Implicit dispatch
    // is the `tools` dispatcher's job — Forge is not a router.
    (void)kEnvOverrides; // .forge env-overrides reader will land with strict mode
    std::cerr << "[forge] unknown command: '" << command << "'\n";
    PrintUsage(std::cerr);
    return kExitUsage;
}
