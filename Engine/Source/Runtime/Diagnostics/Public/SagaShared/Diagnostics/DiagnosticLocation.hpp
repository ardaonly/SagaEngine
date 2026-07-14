/// @file DiagnosticLocation.hpp
/// @brief Shared diagnostic location references without subsystem dependencies.

#pragma once

#include <cstdint>
#include <map>
#include <string>

namespace SagaShared::Diagnostics
{

/// Optional location evidence for source, resource, build, artifact, and related records.
struct DiagnosticLocation
{
    std::string resourceId;       ///< Project/resource identifier, empty when unavailable.
    std::string resourceKind;     ///< Optional resource family such as graph, asset, or package.
    std::string sourcePath;       ///< Source file or document path, empty when unavailable.
    std::uint32_t line = 0;       ///< One-based start line; zero means unknown.
    std::uint32_t column = 0;     ///< One-based start column; zero means unknown.
    std::uint32_t endLine = 0;    ///< One-based end line; zero means unknown.
    std::uint32_t endColumn = 0;  ///< One-based end column; zero means unknown.
    std::string artifactId;       ///< Optional artifact identifier.
    std::string packageId;        ///< Optional package or manifest identifier.
    std::string buildId;          ///< Optional build invocation identifier.
    std::string buildStepId;      ///< Optional build step identifier.
    std::string graphId;          ///< Optional graph identifier.
    std::string graphNodeId;      ///< Optional graph node identifier.
    std::string graphPinId;       ///< Optional graph pin identifier.
    std::map<std::string, std::string> references; ///< Extra stable references keyed by family.
};

} // namespace SagaShared::Diagnostics
