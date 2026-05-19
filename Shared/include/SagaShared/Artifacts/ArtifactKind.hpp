/// @file ArtifactKind.hpp
/// @brief Shared artifact kind vocabulary.

#pragma once

#include <cstdint>

namespace SagaShared::Artifacts
{

/// Stable artifact families used by build, package, runtime, tools, and reports.
enum class ArtifactKind : std::uint8_t
{
    Graph,
    Script,
    Asset,
    Data,
    Manifest,
    DiagnosticReport,
    CanonicalIR,
    GeneratedCode,
    Schema,
    DependencyManifest,
};

} // namespace SagaShared::Artifacts
