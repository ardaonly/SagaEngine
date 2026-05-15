/// @file SagaSdeCompiler.cpp
/// @brief Real SDE compiler bridge used by the Saga product and editor mode.

#include "SagaSdeCompiler.h"

#if SAGA_WITH_SDE
#include "SDE/Compiler/CompilerFacade.h"
#include "SDE/Validation/CompileState.h"
#include "SDE/Validation/Diagnostic.h"
#endif

namespace SagaProduct
{
namespace
{

#if SAGA_WITH_SDE
[[nodiscard]] std::string FormatDiagnostic(const SDE::Diagnostic& diagnostic)
{
    std::string text = diagnostic.code + ": " + diagnostic.message;
    if (!diagnostic.location.file.empty())
    {
        text += " (" + diagnostic.location.file + ")";
    }
    return text;
}
#endif

} // namespace

SagaSdeCompileResult SagaSdeCompiler::Compile(
    const std::filesystem::path& projectRoot)
{
    SagaSdeCompileResult result;

#if SAGA_WITH_SDE
    result.sdeAvailable = true;
    SDE::CompilerFacade compiler;
    SDE::CompileRequest request;
    request.workspaceRoot = projectRoot;
    request.outputRoot = projectRoot / "artifacts";
    const SDE::CompilerFacadeResult compiled = compiler.Compile(request);
    result.state = SDE::StateName(compiled.project.state);

    for (const SDE::Diagnostic& diagnostic : compiled.project.validation.diagnostics)
    {
        result.diagnostics.push_back(FormatDiagnostic(diagnostic));
    }

    result.ok = SDE::IsUsable(compiled.project.state) &&
        compiled.project.graph.has_value();
    if (!result.ok)
    {
        result.message = "SDE compile failed. Previous compiled graph was kept.";
        return result;
    }

    SagaSdeHashes hashes;
    hashes.schemaHash = compiled.project.hashes.schemaHash;
    hashes.dataHash = compiled.project.hashes.dataHash;
    hashes.dependencyHash = compiled.project.hashes.dependencyHash;
    hashes.compiledGraphHash = compiled.project.hashes.compiledGraphHash;
    hashes.artifactHash = compiled.project.hashes.artifactHash;
    result.hashes = hashes;
    m_lastGoodHashes = std::move(hashes);
    result.message = "SDE compile succeeded.";
    return result;
#else
    (void)projectRoot;
    result.sdeAvailable = false;
    result.ok = false;
    result.state = "Disabled";
    result.message = "SDE disabled in this build.";
    return result;
#endif
}

const std::optional<SagaSdeHashes>& SagaSdeCompiler::LastGoodHashes()
    const noexcept
{
    return m_lastGoodHashes;
}

} // namespace SagaProduct
