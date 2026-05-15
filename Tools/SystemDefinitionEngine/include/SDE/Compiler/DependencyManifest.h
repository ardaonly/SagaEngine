/// @file DependencyManifest.h
/// @brief Stable dependency graph metadata for compiled SDE projects.

#pragma once

#include <string>
#include <vector>

namespace SDE
{

struct DependencyRecord
{
    std::string logicalId;
    std::string normalizedPath;
    std::string fingerprint;
};

struct DependencyEdge
{
    std::string fromLogicalId;
    std::string toLogicalId;
};

struct DependencyManifest
{
    std::vector<DependencyRecord> directSchemas;
    std::vector<DependencyRecord> transitiveSchemas;
    std::vector<DependencyRecord> dataFiles;
    std::vector<DependencyEdge>   edges;
};

} // namespace SDE
