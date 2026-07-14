/// @file EditorCompositionManifest.h
/// @brief Manifest model for locating compiled editor composition outputs.

#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

namespace SagaEditor
{

using EditorCompositionManifestVersion = uint32_t;

inline constexpr EditorCompositionManifestVersion kCurrentEditorCompositionManifestVersion = 1;

/// Manifest emitted beside compiled editor composition artifacts.
struct EditorCompositionManifest
{
    std::string manifestKind;
    EditorCompositionManifestVersion manifestVersion = 0;
    std::string compositionId;
    std::filesystem::path artifactPath;
    std::filesystem::path diagnosticsPath;
    std::filesystem::path sourceMapPath;
    std::filesystem::path dependencyManifestPath;
    std::string artifactHash;
    std::string schemaPackage;
    uint32_t schemaPackageVersion = 0;
    std::string compilerVersion;
};

} // namespace SagaEditor
