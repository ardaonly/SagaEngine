/// @file ArtifactManifest.h
/// @brief Stable consumer-facing metadata for emitted SDE artifacts.

#pragma once

#include "SDE/Core/Version.h"

#include <map>
#include <string>
#include <vector>

namespace SDE
{

struct ArtifactOutput
{
    std::string kind;
    std::string path;
    std::string hash;
};

struct ArtifactManifest
{
    ArtifactFormatVersion artifactVersion;
    CompilerVersion       compilerVersion;
    uint32_t              languageVersion = 0;
    std::string           domain;
    std::string           kind;
    std::string           sourceHash;
    std::string           dependencyHash;
    std::map<std::string, std::string> modelHashes;
    std::vector<ArtifactOutput> outputs;
};

} // namespace SDE
