/// @file Version.h
/// @brief Version contracts for schemas, data files, compiler builds, and artifacts.

#pragma once

#include <cstdint>
#include <string>

namespace SDE
{

struct SemanticVersion
{
    uint32_t major = 0;
    uint32_t minor = 0;
    uint32_t patch = 0;

    [[nodiscard]] std::string ToString() const;
};

using SchemaVersion         = SemanticVersion;
using DataVersion           = SemanticVersion;
using LanguageVersion       = SemanticVersion;
using CompilerVersion       = SemanticVersion;
using ArtifactFormatVersion = SemanticVersion;

struct VersionManifest
{
    SchemaVersion         schema;
    DataVersion           data;
    LanguageVersion       language;
    CompilerVersion       compiler;
    ArtifactFormatVersion artifactFormat;
};

[[nodiscard]] bool IsSameMajorCompatible(const SemanticVersion& expected,
                                          const SemanticVersion& actual) noexcept;

[[nodiscard]] CompilerVersion CurrentCompilerVersion() noexcept;
[[nodiscard]] LanguageVersion CurrentLanguageVersion() noexcept;
[[nodiscard]] ArtifactFormatVersion CurrentArtifactFormatVersion() noexcept;

} // namespace SDE
