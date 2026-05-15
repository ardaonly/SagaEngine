/// @file CompilerFacade.cpp
/// @brief Stable high-level compiler facade implementation.

#include "SDE/Compiler/CompilerFacade.h"

#include "SDE/Compiler/CompilerSession.h"

namespace SDE
{
namespace
{

[[nodiscard]] ArtifactManifest MakeManifest(const CompileRequest& request,
                                            const ProjectCompilationResult& project)
{
    ArtifactManifest manifest;
    manifest.artifactVersion = CurrentArtifactFormatVersion();
    manifest.compilerVersion = CurrentCompilerVersion();
    manifest.languageVersion = project.versions.language.major;
    manifest.domain = request.domain;
    manifest.kind = request.artifactKind;
    manifest.sourceHash = project.hashes.sourceHash;
    manifest.dependencyHash = project.hashes.dependencyHash;
    manifest.modelHashes = project.hashes.modelHashes;
    manifest.outputs.push_back({
        "CompiledGraph",
        "graph.json",
        project.hashes.compiledGraphHash,
    });
    manifest.outputs.push_back({
        "Diagnostics",
        "diagnostics.json",
        {},
    });
    manifest.outputs.push_back({
        "Hashes",
        "hashes.json",
        project.hashes.artifactHash,
    });
    return manifest;
}

} // namespace

CompilerFacadeResult CompilerFacade::Validate(const CompileRequest& request) const
{
    CompilerSession session({ request.workspaceRoot });
    CompilerFacadeResult result;
    result.project = session.Validate(request.context);
    result.manifest = MakeManifest(request, result.project);
    return result;
}

CompilerFacadeResult CompilerFacade::Compile(const CompileRequest& request) const
{
    CompilerSession session({ request.workspaceRoot });
    CompilerFacadeResult result;
    result.project = session.Compile(request.context);
    result.manifest = MakeManifest(request, result.project);
    return result;
}

} // namespace SDE
