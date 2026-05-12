/// @file main.cpp
/// @brief sde CLI — validate and compile SDE model projects.

#include "SDE/Compilation/ModelCompiler.h"
#include "SDE/IO/JsonModelLoader.h"
#include "SDE/IO/ModelWriter.h"
#include "SDE/Model/ModelDefinition.h"
#include "SDE/Model/TypeNode.h"
#include "SDE/Validation/Validator.h"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

namespace
{

// ─── Constants ────────────────────────────────────────────────────────────────

constexpr int kExitClean    = 0;
constexpr int kExitWarnings = 1;
constexpr int kExitFail     = 2;
constexpr int kExitUsage    = 3;
constexpr int kExitIOError  = 4;

constexpr const char* kVersion = "0.1.0";

// ─── Printing ─────────────────────────────────────────────────────────────────

void PrintDiagnostics(const std::vector<SDE::Diagnostic>& diagnostics)
{
    for (const auto& d : diagnostics)
    {
        const char* sev = "info";
        switch (d.severity)
        {
            case SDE::Severity::Warning: sev = "warning"; break;
            case SDE::Severity::Error:   sev = "error";   break;
            case SDE::Severity::Fatal:   sev = "fatal";   break;
            default: break;
        }

        // Format: file:line:col: [severity] [code] message
        std::cerr << d.location.file << ':' << d.location.line << ':'
                  << d.location.column << ": [" << sev << "] ["
                  << d.code << "] " << d.message;
        if (!d.suggestion.empty())
            std::cerr << " — " << d.suggestion;
        std::cerr << '\n';
    }
}

int StateToExitCode(SDE::CompileState state)
{
    using SDE::CompileState;
    switch (state)
    {
        case CompileState::Clean:            return kExitClean;
        case CompileState::WithWarnings:     return kExitWarnings;
        case CompileState::UnresolvedRefs:
        case CompileState::ValidationFailed:
        case CompileState::Aborted:          return kExitFail;
        case CompileState::IOError:          return kExitIOError;
    }
    return kExitFail;
}

// ─── Helpers ──────────────────────────────────────────────────────────────────

/// Discover all *.schema.json files under schemas/ in the project directory.
std::vector<std::filesystem::path> FindSchemas(const std::filesystem::path& projectDir)
{
    std::vector<std::filesystem::path> paths;
    auto schemasDir = projectDir / "schemas";
    if (!std::filesystem::exists(schemasDir))
        return paths;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(schemasDir))
    {
        if (entry.is_regular_file() && entry.path().extension() == ".json")
            paths.push_back(entry.path());
    }
    return paths;
}

/// Discover all *.json data files under data/ in the project directory.
std::vector<std::filesystem::path> FindDataFiles(const std::filesystem::path& projectDir)
{
    std::vector<std::filesystem::path> paths;
    auto dataDir = projectDir / "data";
    if (!std::filesystem::exists(dataDir))
        return paths;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(dataDir))
    {
        if (entry.is_regular_file() && entry.path().extension() == ".json")
            paths.push_back(entry.path());
    }
    return paths;
}

/// Load all schemas and data files from a project directory.
/// Returns false and emits diagnostics on I/O failure.
bool LoadProject(const std::filesystem::path&         projectDir,
                  SDE::TypeRegistry&                   types,
                  std::vector<SDE::ModelDefinition>&   definitions,
                  std::vector<SDE::ModelInstance>&     instances,
                  std::vector<SDE::Diagnostic>&        diags)
{
    // ─── Schemas ──────────────────────────────────────────────────────────────
    for (const auto& schemaPath : FindSchemas(projectDir))
    {
        auto def = SDE::JsonModelLoader::LoadDefinition(schemaPath.string(), types, diags);
        if (def)
            definitions.push_back(std::move(*def));
    }

    // ─── Data files ───────────────────────────────────────────────────────────
    SDE::JsonModelLoader loader;
    for (const auto& dataPath : FindDataFiles(projectDir))
    {
        auto loaded = loader.Load(dataPath.string(), diags);
        instances.insert(instances.end(),
                         std::make_move_iterator(loaded.begin()),
                         std::make_move_iterator(loaded.end()));
    }

    return true;
}

// ─── Commands ─────────────────────────────────────────────────────────────────

int CmdValidate(const std::string& projectDir)
{
    SDE::TypeRegistry                types;
    std::vector<SDE::ModelDefinition> defs;
    std::vector<SDE::ModelInstance>   insts;
    std::vector<SDE::Diagnostic>      loadDiags;

    LoadProject(projectDir, types, defs, insts, loadDiags);

    bool hasLoadError = false;
    for (const auto& d : loadDiags)
        if (d.severity == SDE::Severity::Fatal || d.severity == SDE::Severity::Error)
            hasLoadError = true;

    PrintDiagnostics(loadDiags);

    if (hasLoadError)
        return kExitFail;

    types.Freeze();

    SDE::RuleRegistry rules;
    SDE::RuleRegistry::RegisterBuiltIns(rules);
    rules.Freeze();

    SDE::Validator validator(rules, types);
    auto result = validator.Validate(insts, defs);

    PrintDiagnostics(result.diagnostics);

    std::cerr << "validate: " << SDE::StateName(result.state) << '\n';
    return StateToExitCode(result.state);
}

int CmdCompile(const std::string& projectDir, const std::string& outPath)
{
    SDE::TypeRegistry                types;
    std::vector<SDE::ModelDefinition> defs;
    std::vector<SDE::ModelInstance>   insts;
    std::vector<SDE::Diagnostic>      loadDiags;

    LoadProject(projectDir, types, defs, insts, loadDiags);

    bool hasLoadError = false;
    for (const auto& d : loadDiags)
        if (d.severity == SDE::Severity::Fatal || d.severity == SDE::Severity::Error)
            hasLoadError = true;

    PrintDiagnostics(loadDiags);

    if (hasLoadError)
        return kExitFail;

    types.Freeze();

    SDE::RuleRegistry rules;
    SDE::RuleRegistry::RegisterBuiltIns(rules);
    rules.Freeze();

    SDE::ModelCompiler compiler(rules, types);
    auto result = compiler.Compile(std::move(insts), defs);

    PrintDiagnostics(result.validation.diagnostics);

    std::cerr << "compile: " << SDE::StateName(result.state) << '\n';

    if (!result.graph)
        return StateToExitCode(result.state);

    std::string         writeError;
    SDE::JsonModelWriter writer;
    std::string         target = outPath.empty()
                                     ? (std::filesystem::path(projectDir) / "build" / "compiled.json").string()
                                     : outPath;

    // Ensure output directory exists.
    std::filesystem::create_directories(std::filesystem::path(target).parent_path());

    if (!writer.Write(*result.graph, target, &writeError))
    {
        std::cerr << "error: " << writeError << '\n';
        return kExitIOError;
    }

    std::cerr << "output: " << target << '\n';
    return StateToExitCode(result.state);
}

void PrintUsage()
{
    std::cout <<
        "Usage:\n"
        "  sde validate <project-dir>               Validate models; exit 0=clean, 1=warnings, 2=fail\n"
        "  sde compile  <project-dir> [--out=<file>] Compile models to compiled.json\n"
        "  sde --version\n"
        "  sde --help\n"
        "\n"
        "Project directory layout:\n"
        "  <project-dir>/schemas/  *.json  ModelDefinition schema files\n"
        "  <project-dir>/data/     **/*.json  ModelInstance data files\n";
}

bool HasFlag(const std::vector<std::string>& args, const std::string& flag)
{
    for (const auto& a : args)
        if (a == flag) return true;
    return false;
}

std::string TakeOption(const std::vector<std::string>& args, const std::string& prefix)
{
    for (const auto& a : args)
        if (a.substr(0, prefix.size()) == prefix)
            return a.substr(prefix.size());
    return {};
}

} // namespace

// ─── main ─────────────────────────────────────────────────────────────────────

int main(int argc, char* argv[])
{
    std::vector<std::string> args(argv + 1, argv + argc);

    if (args.empty() || HasFlag(args, "--help") || HasFlag(args, "-h"))
    {
        PrintUsage();
        return kExitClean;
    }

    if (HasFlag(args, "--version") || HasFlag(args, "-v"))
    {
        std::cout << "sde " << kVersion << '\n';
        return kExitClean;
    }

    const std::string& command = args[0];

    if (command == "validate")
    {
        if (args.size() < 2)
        {
            std::cerr << "error: sde validate requires a project directory.\n";
            return kExitUsage;
        }
        return CmdValidate(args[1]);
    }

    if (command == "compile")
    {
        if (args.size() < 2)
        {
            std::cerr << "error: sde compile requires a project directory.\n";
            return kExitUsage;
        }
        std::string outPath = TakeOption(args, "--out=");
        return CmdCompile(args[1], outPath);
    }

    std::cerr << "error: unknown command '" << command << "'.\n";
    PrintUsage();
    return kExitUsage;
}
