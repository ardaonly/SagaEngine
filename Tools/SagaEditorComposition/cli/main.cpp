/// @file main.cpp
/// @brief CLI entry point for saga.editor composition artifact compilation.

#include "SagaEditorComposition/CompositionCompiler.h"

#include "SDE/Validation/Diagnostic.h"

#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

namespace
{

constexpr int kExitClean = 0;
constexpr int kExitWarnings = 1;
constexpr int kExitFail = 2;
constexpr int kExitUsage = 3;
constexpr int kExitIOError = 4;

[[nodiscard]] bool HasFlag(const std::vector<std::string>& args,
                           const std::string& flag)
{
    for (const std::string& arg : args)
    {
        if (arg == flag)
        {
            return true;
        }
    }
    return false;
}

[[nodiscard]] std::string TakeValueOption(
    const std::vector<std::string>& args,
    const std::string& name)
{
    for (std::size_t index = 0; index < args.size(); ++index)
    {
        if (args[index] == name && index + 1 < args.size())
        {
            return args[index + 1];
        }
        const std::string prefix = name + "=";
        if (args[index].substr(0, prefix.size()) == prefix)
        {
            return args[index].substr(prefix.size());
        }
    }
    return {};
}

[[nodiscard]] int StateToExitCode(SDE::CompileState state)
{
    switch (state)
    {
        case SDE::CompileState::Clean: return kExitClean;
        case SDE::CompileState::WithWarnings: return kExitWarnings;
        case SDE::CompileState::UnresolvedRefs:
        case SDE::CompileState::ValidationFailed:
        case SDE::CompileState::Aborted: return kExitFail;
        case SDE::CompileState::IOError: return kExitIOError;
    }
    return kExitFail;
}

void PrintDiagnostics(const std::vector<SDE::Diagnostic>& diagnostics)
{
    for (const SDE::Diagnostic& diagnostic : diagnostics)
    {
        std::cerr << diagnostic.location.file << ':' << diagnostic.location.line
                  << ':' << diagnostic.location.column << ": ["
                  << SDE::SeverityName(diagnostic.severity) << "] ["
                  << SDE::DiagnosticCategoryName(diagnostic.category) << "] ["
                  << diagnostic.code << "] " << diagnostic.message << '\n';
    }
}

void PrintUsage()
{
    std::cout
        << "Usage:\n"
        << "  saga-editor-composition-compiler validate --workspace <path> [--composition-id <id>]\n"
        << "  saga-editor-composition-compiler compile --workspace <path> --build-root <path> [--composition-id <id>]\n"
        << "  saga-editor-composition-compiler compile --workspace <path> --out <path> [--composition-id <id>]\n"
        << "  saga-editor-composition-compiler compile --workspace <path> --artifact-out <path> --manifest-out <path>\n"
        << "      --diagnostics-out <path> --source-map-out <path> --dependencies-out <path> [--composition-id <id>]\n"
        << "  saga-editor-composition-compiler --help\n";
}

[[nodiscard]] bool HasAnyExplicitOutput(
    const SagaEditorComposition::CompositionOutputPaths& outputs)
{
    return !outputs.artifactPath.empty() ||
           !outputs.manifestPath.empty() ||
           !outputs.diagnosticsPath.empty() ||
           !outputs.sourceMapPath.empty() ||
           !outputs.dependencyManifestPath.empty();
}

[[nodiscard]] bool HasCompleteExplicitOutputs(
    const SagaEditorComposition::CompositionOutputPaths& outputs)
{
    return !outputs.artifactPath.empty() &&
           !outputs.manifestPath.empty() &&
           !outputs.diagnosticsPath.empty() &&
           !outputs.sourceMapPath.empty() &&
           !outputs.dependencyManifestPath.empty();
}

[[nodiscard]] SagaEditorComposition::CompositionCompileRequest MakeRequest(
    const std::vector<std::string>& args)
{
    SagaEditorComposition::CompositionCompileRequest request;
    request.workspaceRoot = TakeValueOption(args, "--workspace");
    request.buildRoot = TakeValueOption(args, "--build-root");
    request.outputRoot = TakeValueOption(args, "--out");
    request.explicitOutputs.artifactPath = TakeValueOption(args, "--artifact-out");
    request.explicitOutputs.manifestPath = TakeValueOption(args, "--manifest-out");
    request.explicitOutputs.diagnosticsPath = TakeValueOption(args, "--diagnostics-out");
    request.explicitOutputs.sourceMapPath = TakeValueOption(args, "--source-map-out");
    request.explicitOutputs.dependencyManifestPath = TakeValueOption(args, "--dependencies-out");
    request.compositionId = TakeValueOption(args, "--composition-id");
    return request;
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

    const std::string command = args[0];
    if (command != "validate" && command != "compile")
    {
        std::cerr << "error: unknown command '" << command << "'.\n";
        PrintUsage();
        return kExitUsage;
    }

    SagaEditorComposition::CompositionCompileRequest request = MakeRequest(args);
    if (request.workspaceRoot.empty())
    {
        std::cerr << "error: --workspace is required.\n";
        return kExitUsage;
    }
    if (command == "compile" &&
        request.buildRoot.empty() &&
        request.outputRoot.empty() &&
        !HasCompleteExplicitOutputs(request.explicitOutputs))
    {
        std::cerr << "error: compile requires --build-root, --out, or the complete explicit output contract.\n";
        return kExitUsage;
    }
    if (command == "compile" &&
        ((!request.buildRoot.empty() && !request.outputRoot.empty()) ||
         (!request.buildRoot.empty() && HasAnyExplicitOutput(request.explicitOutputs)) ||
         (!request.outputRoot.empty() && HasAnyExplicitOutput(request.explicitOutputs))))
    {
        std::cerr << "error: compile accepts only one output mode: --build-root, --out, or explicit output paths.\n";
        return kExitUsage;
    }
    if (command == "compile" &&
        HasAnyExplicitOutput(request.explicitOutputs) &&
        !HasCompleteExplicitOutputs(request.explicitOutputs))
    {
        std::cerr << "error: explicit output contract requires --artifact-out, --manifest-out, --diagnostics-out, --source-map-out, and --dependencies-out.\n";
        return kExitUsage;
    }

    SagaEditorComposition::CompositionCompiler compiler;
    const SagaEditorComposition::CompositionCompileResult result =
        command == "compile" ? compiler.Compile(request) : compiler.Validate(request);

    PrintDiagnostics(result.diagnostics);
    std::cerr << command << ": " << SDE::StateName(result.state) << '\n';
    if (command == "compile")
    {
        std::cerr << "artifact: " << result.outputs.artifactPath << '\n';
        std::cerr << "manifest: " << result.outputs.manifestPath << '\n';
        std::cerr << "artifactHash: " << result.hashes.artifactHash << '\n';
    }
    return StateToExitCode(result.state);
}
