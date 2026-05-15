/// @file CompileResult.h
/// @brief Stable high-level SDE compile result contract.

#pragma once

#include "SDE/Artifacts/ArtifactManifest.h"
#include "SDE/Compilation/CompiledModelGraph.h"
#include "SDE/Compiler/DependencyManifest.h"
#include "SDE/Core/Version.h"
#include "SDE/Validation/ValidationResult.h"

#include <map>
#include <optional>
#include <string>

namespace SDE
{

struct CompilerHashes
{
    std::string sourceHash;
    std::string schemaHash;
    std::string dataHash;
    std::string dependencyHash;
    std::string compiledGraphHash;
    std::string artifactHash;
    std::map<std::string, std::string> modelHashes;
};

struct ProjectCompilationResult
{
    CompileState                      state = CompileState::Clean;
    std::optional<CompiledModelGraph> graph;
    ValidationResult                  validation;
    VersionManifest                   versions;
    DependencyManifest                dependencies;
    CompilerHashes                    hashes;
    bool                              cancelled = false;
};

struct CompilerFacadeResult
{
    ProjectCompilationResult project;
    ArtifactManifest         manifest;
};

} // namespace SDE
