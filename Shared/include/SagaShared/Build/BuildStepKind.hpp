/// @file BuildStepKind.hpp
/// @brief Shared build step kind vocabulary.

#pragma once

#include <cstdint>

namespace SagaShared::Build
{

/// Neutral build pipeline step families.
enum class BuildStepKind : std::uint8_t
{
    Unknown,
    Configure,
    CompileSDE,
    ValidateGraph,
    CompileScript,
    CookAsset,
    RunPrism,
    GenerateArtifactManifest,
    StagePackage,
    GeneratePackageManifest,
    ValidatePackageManifest,
    PublishReadinessCheck,
    WriteBuildReport,
};

} // namespace SagaShared::Build
