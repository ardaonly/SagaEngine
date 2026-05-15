/// @file main.cpp
/// @brief sde CLI — validate and compile SDE model projects.

#include "SDE/Compiler/CompilerFacade.h"
#include "SDE/IO/ModelWriter.h"

#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace
{

constexpr int kExitClean    = 0;
constexpr int kExitWarnings = 1;
constexpr int kExitFail     = 2;
constexpr int kExitUsage    = 3;
constexpr int kExitIOError  = 4;

constexpr const char* kVersion = "0.1.1";

void PrintDiagnostics(const std::vector<SDE::Diagnostic>& diagnostics)
{
    for (const auto& d : diagnostics)
    {
        std::cerr << d.location.file << ':' << d.location.line << ':'
                  << d.location.column << ": [" << SDE::SeverityName(d.severity)
                  << "] [" << SDE::DiagnosticCategoryName(d.category)
                  << "] [" << d.code << "] " << d.message;
        if (!d.suggestion.empty())
            std::cerr << " - " << d.suggestion;
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

std::string TakeValueOption(const std::vector<std::string>& args,
                            const std::string& name)
{
    for (std::size_t i = 0; i < args.size(); ++i)
    {
        if (args[i] == name && i + 1 < args.size())
            return args[i + 1];
        const std::string prefix = name + "=";
        if (args[i].substr(0, prefix.size()) == prefix)
            return args[i].substr(prefix.size());
    }
    return {};
}

SDE::CompileRequest MakeRequest(const std::string& workspace,
                                const std::string& out)
{
    SDE::CompileRequest request;
    request.workspaceRoot = std::filesystem::path(workspace);
    request.outputRoot = out.empty()
        ? request.workspaceRoot / "artifacts"
        : std::filesystem::path(out);
    return request;
}

void WriteJson(const std::filesystem::path& path, const nlohmann::json& json)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::trunc);
    out << json.dump(2);
}

nlohmann::json ManifestJson(const SDE::ArtifactManifest& manifest)
{
    nlohmann::json json;
    json["artifactVersion"] = manifest.artifactVersion.major;
    json["compilerVersion"] = manifest.compilerVersion.ToString();
    json["languageVersion"] = manifest.languageVersion;
    json["domain"] = manifest.domain;
    json["kind"] = manifest.kind;
    json["sourceHash"] = manifest.sourceHash;
    json["dependencyHash"] = manifest.dependencyHash;
    json["modelHashes"] = manifest.modelHashes;
    json["outputs"] = nlohmann::json::array();
    for (const auto& output : manifest.outputs)
    {
        json["outputs"].push_back({
            {"kind", output.kind},
            {"path", output.path},
            {"hash", output.hash},
        });
    }
    return json;
}

nlohmann::json DiagnosticsJson(const std::vector<SDE::Diagnostic>& diagnostics)
{
    nlohmann::json json = nlohmann::json::array();
    for (const auto& d : diagnostics)
    {
        json.push_back({
            {"severity", SDE::SeverityName(d.severity)},
            {"category", SDE::DiagnosticCategoryName(d.category)},
            {"code", d.code},
            {"message", d.message},
            {"file", d.location.file},
            {"line", d.location.line},
            {"column", d.location.column},
            {"metadata", d.metadata},
        });
    }
    return json;
}

int CmdValidate(const std::string& workspace)
{
    SDE::CompilerFacade compiler;
    auto result = compiler.Validate(MakeRequest(workspace, {}));

    PrintDiagnostics(result.project.validation.diagnostics);
    std::cerr << "validate: " << SDE::StateName(result.project.state) << '\n';
    return StateToExitCode(result.project.state);
}

int CmdCompile(const std::string& workspace, const std::string& outPath)
{
    SDE::CompilerFacade compiler;
    SDE::CompileRequest request = MakeRequest(workspace, outPath);
    auto result = compiler.Compile(request);

    PrintDiagnostics(result.project.validation.diagnostics);
    std::cerr << "compile: " << SDE::StateName(result.project.state) << '\n';

    WriteJson(request.outputRoot / "diagnostics.json",
              DiagnosticsJson(result.project.validation.diagnostics));
    WriteJson(request.outputRoot / "manifest.json", ManifestJson(result.manifest));
    WriteJson(request.outputRoot / "hashes.json", {
        {"sourceHash", result.project.hashes.sourceHash},
        {"schemaHash", result.project.hashes.schemaHash},
        {"dataHash", result.project.hashes.dataHash},
        {"dependencyHash", result.project.hashes.dependencyHash},
        {"compiledGraphHash", result.project.hashes.compiledGraphHash},
        {"artifactHash", result.project.hashes.artifactHash},
        {"modelHashes", result.project.hashes.modelHashes},
    });

    if (!result.project.graph)
        return StateToExitCode(result.project.state);

    std::string writeError;
    SDE::JsonModelWriter writer;
    const std::filesystem::path graphPath = request.outputRoot / "graph.json";
    if (!writer.Write(*result.project.graph, graphPath.string(), &writeError))
    {
        std::cerr << "error: " << writeError << '\n';
        return kExitIOError;
    }

    std::cerr << "output: " << request.outputRoot << '\n';
    std::cerr << "compiledGraphHash: " << result.project.hashes.compiledGraphHash << '\n';
    std::cerr << "artifactHash: " << result.project.hashes.artifactHash << '\n';
    return StateToExitCode(result.project.state);
}

void PrintUsage()
{
    std::cout <<
        "Usage:\n"
        "  sde validate --workspace <path>          Validate native .sde source or legacy JSON models\n"
        "  sde compile  --workspace <path> --out <path> Compile deterministic artifacts\n"
        "  sde inspect  --workspace <path>          Print workspace artifact summary\n"
        "  sde --version\n"
        "  sde --help\n"
        "\n"
        "Project directory layout:\n"
        "  <workspace>/source/  **/*.sde  Native authored SDE source files\n"
        "  <workspace>/schemas/ *.json    Legacy ModelDefinition schema files\n"
        "  <workspace>/data/    **/*.json Legacy ModelInstance data files\n";
}

bool HasFlag(const std::vector<std::string>& args, const std::string& flag)
{
    for (const auto& arg : args)
        if (arg == flag)
            return true;
    return false;
}

std::string TakeOption(const std::vector<std::string>& args, const std::string& prefix)
{
    for (const auto& arg : args)
        if (arg.substr(0, prefix.size()) == prefix)
            return arg.substr(prefix.size());
    return {};
}

} // namespace

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
        const std::string workspace = TakeValueOption(args, "--workspace");
        const std::string positional = args.size() >= 2 && args[1].rfind("--", 0) != 0
            ? args[1]
            : std::string{};
        const std::string target = workspace.empty() ? positional : workspace;
        if (target.empty())
        {
            std::cerr << "error: sde validate requires --workspace <path>.\n";
            return kExitUsage;
        }
        return CmdValidate(target);
    }

    if (command == "compile")
    {
        const std::string workspace = TakeValueOption(args, "--workspace");
        const std::string positional = args.size() >= 2 && args[1].rfind("--", 0) != 0
            ? args[1]
            : std::string{};
        const std::string target = workspace.empty() ? positional : workspace;
        if (target.empty())
        {
            std::cerr << "error: sde compile requires --workspace <path>.\n";
            return kExitUsage;
        }
        const std::string out = TakeValueOption(args, "--out").empty()
            ? TakeOption(args, "--out=")
            : TakeValueOption(args, "--out");
        return CmdCompile(target, out);
    }

    if (command == "inspect")
    {
        const std::string workspace = TakeValueOption(args, "--workspace");
        if (workspace.empty())
        {
            std::cerr << "error: sde inspect requires --workspace <path>.\n";
            return kExitUsage;
        }
        SDE::CompilerFacade compiler;
        const auto result = compiler.Validate(MakeRequest(workspace, {}));
        std::cout << "state: " << SDE::StateName(result.project.state) << '\n';
        std::cout << "domain: " << result.manifest.domain << '\n';
        std::cout << "kind: " << result.manifest.kind << '\n';
        std::cout << "sourceHash: " << result.manifest.sourceHash << '\n';
        std::cout << "dependencyHash: " << result.manifest.dependencyHash << '\n';
        return StateToExitCode(result.project.state);
    }

    std::cerr << "error: unknown command '" << command << "'.\n";
    PrintUsage();
    return kExitUsage;
}
