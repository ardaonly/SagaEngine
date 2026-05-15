/// @file EnvProbe.h
/// @brief Detect and report the versions of tools that Forge depends on.
///
/// Used by `forge env` and strict-mode version enforcement.

#pragma once

#include "BuildModel.h"
#include "ToolEnv.h"
#include <iosfwd>
#include <string>

namespace Forge
{

// ─── ToolInfo ─────────────────────────────────────────────────────────────────

struct ToolInfo
{
    std::string name;       ///<  Executable name actually queried
    std::string version;    ///<  Parsed "X.Y.Z" (or best effort) from --version output
    bool        found = false;
};

// ─── EnvReport ────────────────────────────────────────────────────────────────

struct EnvReport
{
    ToolInfo cmake;
    ToolInfo conan;
    ToolInfo cxx;   ///<  First of c++ / g++ / clang++ found on PATH
};

// ─── EnvProbe ─────────────────────────────────────────────────────────────────

class EnvProbe
{
public:
    EnvProbe()                             = delete;
    EnvProbe(const EnvProbe&)              = delete;
    EnvProbe& operator=(const EnvProbe&)   = delete;

    /// Query tool versions using the executable names from env.
    [[nodiscard]] static EnvReport Detect(const ToolEnv& env);

    /// Print a human-readable table. Marks mismatches against forge.toml pins.
    static void PrintTable(const EnvReport&  report,
                           const BuildModel& model,
                           std::ostream&     os);

    /// Print a JSON object suitable for scripting.
    static void PrintJson(const EnvReport&  report,
                          const BuildModel& model,
                          std::ostream&     os);

    /// Return true if all pinned versions in model.toolchain are satisfied.
    /// Emits a diagnostic per mismatch to stderr.
    [[nodiscard]] static bool CheckStrict(const EnvReport&  report,
                                          const BuildModel& model);
};

} // namespace Forge
