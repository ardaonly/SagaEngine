/// @file main.cpp
/// @brief prism-extract — ClangTool entry point, option parsing, and JSON emission.
///
/// Usage:
///   prism-extract --repo-root <abs-path>
///                 -p <build-dir>
///                 [-o <output.json>]
///                 [source-files...]
///
/// Behavior:
///   - If source files are provided, only those translation units are processed.
///   - If no source files are provided, every translation unit listed in the
///     compilation database is processed.
///   - Partial parse failures still emit JSON because incomplete graph context
///     is better than no graph context.

#include "Pipeline/Emit/JsonEmitter.h"
#include "Pipeline/PrismAction.h"
#include "Record.h"

#include <clang/Tooling/CommonOptionsParser.h>
#include <clang/Tooling/CompilationDatabase.h>
#include <clang/Tooling/Tooling.h>

#include <llvm/Support/CommandLine.h>
#include <llvm/Support/raw_ostream.h>

#include <algorithm>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iostream>
#include <mutex>
#include <string>
#include <vector>

namespace fs = std::filesystem;

// ─── CLI Option Category ─────────────────────────────────────────────────────

static llvm::cl::OptionCategory g_cat("prism-extract options");

static llvm::cl::opt<std::string> g_repoRoot(
    "repo-root",
    llvm::cl::desc("Absolute path to the repository root."),
    llvm::cl::value_desc("path"),
    llvm::cl::Required,
    llvm::cl::cat(g_cat));

static llvm::cl::opt<std::string> g_output(
    "o",
    llvm::cl::desc("Output raw JSON path. Default: prism.raw.json."),
    llvm::cl::value_desc("file"),
    llvm::cl::init("prism.raw.json"),
    llvm::cl::cat(g_cat));

// ─── Helpers ─────────────────────────────────────────────────────────────────

static fs::path AbsolutePath(const std::string& path)
{
    std::error_code ec;

    fs::path abs = fs::absolute(fs::path(path), ec);
    if (ec)
        return fs::path(path);

    return abs.lexically_normal();
}

static bool IsDirectory(const fs::path& path)
{
    std::error_code ec;
    return fs::is_directory(path, ec);
}

static std::vector<std::string> UniqueSorted(std::vector<std::string> values)
{
    std::sort(values.begin(), values.end());
    values.erase(std::unique(values.begin(), values.end()), values.end());
    return values;
}

static std::vector<std::string> ResolveSourceFiles(
    clang::tooling::CommonOptionsParser& options_parser)
{
    std::vector<std::string> sources = options_parser.getSourcePathList();

    if (!sources.empty())
        return UniqueSorted(std::move(sources));

    // No explicit positional source files were provided.
    // In CI/default mode, process every translation unit known by the
    // compilation database.
    for (const clang::tooling::CompileCommand& command :
         options_parser.getCompilations().getAllCompileCommands())
    {
        if (!command.Filename.empty())
            sources.push_back(command.Filename);
    }

    return UniqueSorted(std::move(sources));
}

static int PrintNoSourcesError()
{
    llvm::errs()
        << "prism-extract: no source files to process.\n"
        << "prism-extract: either pass source files explicitly or provide a valid "
           "compile_commands.json with -p <build-dir>.\n";

    return EXIT_FAILURE;
}

static int PrintRepoRootError(const fs::path& repo_root)
{
    llvm::errs()
        << "prism-extract: --repo-root is not a directory: "
        << repo_root.string()
        << '\n';

    return EXIT_FAILURE;
}

static int PrintOutputPathError(const fs::path& output_path)
{
    llvm::errs()
        << "prism-extract: output path points to a directory: "
        << output_path.string()
        << '\n';

    return EXIT_FAILURE;
}

// ─── Entry Point ─────────────────────────────────────────────────────────────

int main(int argc, const char** argv)
{
    auto expected_parser = clang::tooling::CommonOptionsParser::create(
        argc,
        argv,
        g_cat,
        llvm::cl::ZeroOrMore);

    if (!expected_parser)
    {
        llvm::errs()
            << "prism-extract: "
            << llvm::toString(expected_parser.takeError())
            << '\n';

        return EXIT_FAILURE;
    }

    clang::tooling::CommonOptionsParser& options_parser = *expected_parser;

    // ── Validate repository root ─────────────────────────────────────────────

    const fs::path repo_root = AbsolutePath(g_repoRoot.getValue());

    if (!IsDirectory(repo_root))
        return PrintRepoRootError(repo_root);

    // ── Resolve source files ─────────────────────────────────────────────────

    std::vector<std::string> source_files = ResolveSourceFiles(options_parser);

    if (source_files.empty())
        return PrintNoSourcesError();

    // ── Resolve output path ──────────────────────────────────────────────────

    const fs::path output_path = AbsolutePath(g_output.getValue());

    if (IsDirectory(output_path))
        return PrintOutputPathError(output_path);

    // ── Run ClangTool ────────────────────────────────────────────────────────

    clang::tooling::ClangTool tool(
        options_parser.getCompilations(),
        source_files);

    prism::ResultAccumulator accumulator;
    prism::PrismActionFactory factory(repo_root.string(), accumulator);

    llvm::errs()
        << "prism-extract: repo root: "
        << repo_root.string()
        << '\n';

    llvm::errs()
        << "prism-extract: output: "
        << output_path.string()
        << '\n';

    llvm::errs()
        << "prism-extract: translation units: "
        << source_files.size()
        << '\n';

    const int tool_result = tool.run(&factory);

    if (tool_result != 0)
    {
        llvm::errs()
            << "prism-extract: warning: some translation units failed to parse; "
               "partial results will still be emitted.\n";
    }

    // ── Collect Results ──────────────────────────────────────────────────────

    prism::ExtractionResult result;

    {
        std::lock_guard<std::mutex> lock(accumulator.mu);

        result.files = std::move(accumulator.files);
        result.symbols = std::move(accumulator.symbols);
    }

    // ── Emit JSON ────────────────────────────────────────────────────────────

    try
    {
        prism::EmitJson(result, repo_root.string(), output_path);
    }
    catch (const std::exception& exc)
    {
        llvm::errs()
            << "prism-extract: emit error: "
            << exc.what()
            << '\n';

        return EXIT_FAILURE;
    }

    std::cout
        << "[OK] prism-extract — "
        << result.symbols.size()
        << " symbols  "
        << result.files.size()
        << " files  "
        << "→ "
        << output_path.string()
        << '\n';

    return tool_result == 0 ? EXIT_SUCCESS : tool_result;
}
