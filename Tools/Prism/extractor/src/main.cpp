/// @file main.cpp
/// @brief prism-extract — ClangTool entry point, option parsing, and JSON emission.
///
/// Usage:
///   prism-extract -p <build-dir> [source-files…]
///                 --repo-root <abs-path>
///                 [-o <output.json>]
///
/// When no source files are given, all translation units listed in
/// compile_commands.json are processed. This is the default CI mode.

#include "Pipeline/Emit/JsonEmitter.h"
#include "Pipeline/PrismAction.h"
#include "Record.h"

#include <clang/Tooling/CommonOptionsParser.h>
#include <clang/Tooling/Tooling.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/raw_ostream.h>

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <mutex>

namespace fs = std::filesystem;

// ─── CLI Option Category ──────────────────────────────────────────────────────

static llvm::cl::OptionCategory g_cat("prism-extract options");

static llvm::cl::opt<std::string> g_repoRoot(
    "repo-root",
    llvm::cl::desc("Absolute path to the repository root (required)."),
    llvm::cl::value_desc("path"),
    llvm::cl::Required,
    llvm::cl::cat(g_cat));

static llvm::cl::opt<std::string> g_output(
    "o",
    llvm::cl::desc("Output JSON path (default: prism.raw.json)."),
    llvm::cl::value_desc("file"),
    llvm::cl::init("prism.raw.json"),
    llvm::cl::cat(g_cat));

// ─── Entry Point ──────────────────────────────────────────────────────────────

int main(int argc, const char** argv)
{
    // CommonOptionsParser handles -p / --extra-arg / source file list.
    auto expected_parser = clang::tooling::CommonOptionsParser::create(
        argc, argv, g_cat, llvm::cl::OneOrMore);

    if (!expected_parser)
    {
        llvm::errs() << "prism-extract: " << llvm::toString(expected_parser.takeError()) << '\n';
        return EXIT_FAILURE;
    }

    clang::tooling::CommonOptionsParser& options_parser = *expected_parser;

    // ── Validate repository root ──────────────────────────────────────────────

    const fs::path repo_root = fs::absolute(g_repoRoot.getValue());
    if (!fs::is_directory(repo_root))
    {
        llvm::errs() << "prism-extract: --repo-root is not a directory: "
                     << repo_root.string() << '\n';
        return EXIT_FAILURE;
    }

    // ── Run the tool ──────────────────────────────────────────────────────────

    clang::tooling::ClangTool tool(
        options_parser.getCompilations(),
        options_parser.getSourcePathList());

    prism::ResultAccumulator accumulator;
    prism::PrismActionFactory factory(repo_root.string(), accumulator);

    const int tool_result = tool.run(&factory);

    // Partial results are still valuable — emit even when some TUs failed.
    if (tool_result != 0)
        llvm::errs() << "prism-extract: warning — some translation units had errors.\n";

    // ── Collect and emit ──────────────────────────────────────────────────────

    prism::ExtractionResult result;
    {
        std::lock_guard<std::mutex> lock(accumulator.mu);
        result.files   = std::move(accumulator.files);
        result.symbols = std::move(accumulator.symbols);
    }

    try
    {
        const fs::path out_path(g_output.getValue());
        prism::EmitJson(result, repo_root.string(), out_path);

        std::cout
            << "[OK] prism-extract — "
            << result.symbols.size() << " symbols  "
            << result.files.size()   << " files"
            << " → " << out_path.string() << '\n';
    }
    catch (const std::exception& exc)
    {
        llvm::errs() << "prism-extract: emit error — " << exc.what() << '\n';
        return EXIT_FAILURE;
    }

    return tool_result;
}
