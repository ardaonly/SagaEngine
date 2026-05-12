/// @file BuildModel.h
/// @brief Backend-agnostic intermediate representation of a build request.
///
/// All CLI input and forge.toml data is lowered into a BuildModel before any
/// backend adapter sees it. This keeps the adapter interface stable regardless
/// of how the user expressed the request (flags, manifest, or both).

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace Forge
{

class Manifest;

// ─── Constituent types ────────────────────────────────────────────────────────

struct ToolchainConfig
{
    std::string cmake;    ///<  Required cmake version (advisory; enforcement is future work)
    std::string conan;    ///<  Required conan version
    std::string compiler; ///<  Required compiler (e.g. "clang-17")
};

struct BuildConfig
{
    std::string sourceDir = ".";    ///<  CMake source root (-S)
    std::string buildDir  = "build"; ///<  CMake binary directory (-B / --build)
    std::string preset;              ///<  CMake preset name (--preset); takes priority over sourceDir/buildDir in configure
    std::string config;              ///<  Build configuration (Release / Debug …)
    std::string target;              ///<  Optional build target
    uint32_t    jobs      = 0;      ///<  Parallel job count (0 = auto-calculate based on policy)
};

struct Dependency
{
    std::string name;
    std::string version;
};

// ─── BuildModel ───────────────────────────────────────────────────────────────

/// Complete, backend-neutral description of what to build and how.
///
/// CLI commands construct a BuildModel from the manifest, then apply
/// flag overrides before handing it to the appropriate adapter.
struct BuildModel
{
    std::string     projectName;
    std::string     projectVersion;
    ToolchainConfig toolchain;
    BuildConfig     build;
    std::vector<Dependency> deps;

    /// Populate from a loaded manifest. All fields default to sensible
    /// values so callers can safely apply overrides with assignment.
    [[nodiscard]] static BuildModel FromManifest(const Manifest& m);
};

} // namespace Forge
