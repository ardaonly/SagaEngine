/// @file StableHash.h
/// @brief Platform-independent hashing helpers for SDE compiler artifacts.

#pragma once

#include <filesystem>
#include <string>

namespace SDE
{

class CompiledModelGraph;
struct DependencyManifest;

[[nodiscard]] std::string NormalizePathForArtifactHash(const std::filesystem::path& root,
                                                        const std::filesystem::path& path);
[[nodiscard]] std::string StableHashBytes(const std::string& bytes);
[[nodiscard]] std::string StableFileFingerprint(const std::filesystem::path& path);
[[nodiscard]] std::string StableHashCompiledGraph(const CompiledModelGraph& graph);
[[nodiscard]] std::string StableHashDependencyManifest(const DependencyManifest& manifest);

} // namespace SDE
