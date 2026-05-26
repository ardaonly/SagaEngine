/// @file CMakeAdapter.h
/// @brief CMake and CTest backend adapter — lowers a BuildModel into cmake/ctest invocations.
///
/// All cmake and ctest process spawning is centralised here.

#pragma once

#include "BuildModel.h"
#include <cstdint>
#include <string>
#include <vector>

namespace Forge
{

// ─── CMakeAdapter ─────────────────────────────────────────────────────────────

class CMakeAdapter
{
public:
    CMakeAdapter()                               = delete;
    CMakeAdapter(const CMakeAdapter&)            = delete;
    CMakeAdapter& operator=(const CMakeAdapter&) = delete;

    // ─── Options ──────────────────────────────────────────────────────────────

    struct TestOptions
    {
        std::string label;        ///<  --label-regex
        std::string labelExclude; ///<  --label-exclude
        std::uint32_t jobs = 0;   ///<  ctest -j, 0 leaves CTest default behavior
        bool verbose = false;
    };

    struct InstallOptions
    {
        std::string prefix;     ///<  --prefix
        std::string component;  ///<  --component
    };

    // ─── Build lifecycle ──────────────────────────────────────────────────────

    /// Configure. Uses `cmake --preset <name>` when preset is set; falls back to -S/-B.
    [[nodiscard]] static int Configure(const BuildModel&              model,
                                       const std::vector<std::string>& extra   = {},
                                       bool                            explain = false);

    /// Build.
    [[nodiscard]] static int Build(const BuildModel&              model,
                                   const std::vector<std::string>& extra   = {},
                                   bool                            explain = false);

    /// Run the CTest suite for the project.
    [[nodiscard]] static int Test(const BuildModel&               model,
                                  const TestOptions&              opts,
                                  const std::vector<std::string>& extra   = {},
                                  bool                            explain = false);

    /// Install built artifacts (`cmake --install`).
    [[nodiscard]] static int InstallTarget(const BuildModel&               model,
                                           const InstallOptions&            opts,
                                           const std::vector<std::string>&  extra   = {},
                                           bool                             explain = false);

    /// Print available CMake presets (`cmake --list-presets [type]`).
    /// type: empty = configure presets, "build", or "test".
    [[nodiscard]] static int ListPresets(const std::string&             type  = "",
                                         const std::vector<std::string>& extra = {});

    /// Remove the build directory.
    [[nodiscard]] static int Clean(const BuildModel& model, bool explain = false);
};

} // namespace Forge
